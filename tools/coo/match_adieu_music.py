"""Compare packaged Adieu music stems to the reference's actual audio.

This is reproducible acoustic evidence, not a listening claim. The report names
candidate matches and scores; low scores and dialogue/SFX masking stay unknown.
Requires scipy, soundfile, faster-whisper (decode only), and a vgmstream CLI.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', required=True)
    parser.add_argument('--decoder', required=True, type=Path)
    parser.add_argument('--out', type=Path, default=Path('build/coo/adieu-research'))
    args = parser.parse_args()
    os.environ['DAWN_GAME_ROOT'] = args.game_root
    import package_read as packages
    from extract_gateway_bindings import array, u32
    import numpy as np
    from scipy import signal
    from faster_whisper.audio import decode_audio
    directory = args.out / 'music'
    directory.mkdir(exist_ok=True, parents=True)
    _, bank = packages.read(0x80BF5FD8)
    stems = []
    for at in array(bank, 24, 4, 0x80800014):
        tag = u32(bank, at)
        media_id, raw = packages.read(tag)
        assert raw[:4] == b'RIFF'
        wem = directory / f'{tag:08X}.wem'
        wav = wem.with_suffix('.wav')
        wem.write_bytes(raw)
        if not wav.is_file():
            subprocess.run([str(args.decoder.resolve()), '-i', '-o', str(wav), str(wem)],
                           stdout=subprocess.DEVNULL, check=True)
        stems.append(dict(tag=tag, mediaId=media_id, sha256=hashlib.sha256(raw).hexdigest(), wav=wav))
    rate, hop, bins = 16000, 2000, 64
    edges = np.geomspace(80, 7500, bins + 1)

    def features(path):
        x = decode_audio(str(path), sampling_rate=rate)
        frequency, _, spectrum = signal.stft(x, rate, nperseg=2048,
                                             noverlap=2048 - hop, boundary=None)
        magnitude = np.abs(spectrum)
        bands = []
        for lo, hi in zip(edges, edges[1:]):
            selected = magnitude[(frequency >= lo) & (frequency < hi)]
            bands.append(selected.mean(axis=0) if len(selected) else magnitude[np.argmin(abs(frequency - (lo + hi) / 2))])
        bands = np.stack(bands)
        # Log-band whitening suppresses volume and EQ differences in the upload.
        bands = np.log(np.maximum(bands, 1e-7))
        bands -= bands.mean(axis=0, keepdims=True)
        return bands, len(x) / rate, x

    reference, duration, reference_pcm = features(args.out / 'video/reference.f140.m4a')
    matches = []
    window = 64  # eight seconds; one-second storyboard gets overlapping matches
    for stem in stems:
        source, seconds, pcm = features(stem.pop('wav'))
        stem['durationSeconds'] = seconds
        for offset in range(0, source.shape[1] - window + 1, window):
            samples = pcm[offset * hop:(offset + window) * hop]
            rms = float(np.sqrt(np.mean(samples * samples)))
            if rms < 0.001:  # below -60 dBFS: silence/codec residue is not a cue
                continue
            sample = source[:, offset:offset + window].copy()
            # Center each band over the comparison window to reject static timbre.
            sample -= sample.mean(axis=1, keepdims=True)
            norm = np.linalg.norm(sample)
            if norm < 1e-4:
                continue
            dot = signal.fftconvolve(reference, sample[:, ::-1], mode='valid', axes=1).sum(axis=0)
            ones = np.ones(window)
            sum_x = signal.fftconvolve(reference, ones[None, :], mode='valid', axes=1)
            sum_x2 = signal.fftconvolve(reference * reference, ones[None, :], mode='valid', axes=1)
            denominator = norm * np.sqrt(np.maximum((sum_x2 - sum_x * sum_x / window).sum(axis=0), 1e-8))
            scores = dot / denominator
            assert np.isfinite(scores).all()
            best = int(np.argmax(scores))
            matches.append(dict(tag=stem['tag'], sourceStart=offset * hop / rate,
                referenceStart=best * hop / rate + 2048 / (2 * rate),
                windowSeconds=window * hop / rate, sourceRmsDbfs=round(20 * float(np.log10(rms)), 2),
                score=round(float(scores[best]), 5)))
        print(f'{stem["tag"]:08X} {seconds:.2f}s matched', flush=True)
    report = dict(method='Eight-second normalized log-frequency spectrogram correlation at 125ms hops',
        limitation='Acoustic candidates; masks and multiple stems can lower scores. No auditory review asserted.',
        bank=0x80BF5FD8, referenceDuration=duration, stems=stems,
        matches=sorted(matches, key=lambda m: m['referenceStart']))
    (directory / 'acoustic-matches.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
