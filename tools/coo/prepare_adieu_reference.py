"""Build reproducible one-second visual/audio evidence for the supplied walkthrough.

Frames and automated speech timing are observations, not mission completion tests.
Speech recognition is explicitly unverified until checked against the recording.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path('build/coo/adieu-research/video'))
    parser.add_argument('--transcribe', action='store_true')
    args = parser.parse_args()
    directory = args.directory.resolve()
    video, audio = directory / 'reference.f298.mp4', directory / 'reference.f140.m4a'
    if args.transcribe:
        from faster_whisper import WhisperModel
        model = WhisperModel('small.en', device='cpu', compute_type='int8', cpu_threads=6)
        segments, info = model.transcribe(str(audio), language='en', vad_filter=True, word_timestamps=True)
        rows = []
        for segment in segments:
            rows.append({'start': segment.start, 'end': segment.end, 'text': segment.text,
                         'words': [{'start': w.start, 'end': w.end, 'word': w.word, 'probability': w.probability} for w in segment.words]})
            print(f'{segment.start:.2f}-{segment.end:.2f}', flush=True)
        (directory / 'speech-machine.json').write_text(json.dumps({'method': 'faster-whisper small.en int8; machine timing, not human verification',
            'languageProbability': info.language_probability, 'segments': rows}, indent=2), encoding='utf-8')
        return
    import imageio_ffmpeg
    from PIL import Image, ImageDraw, ImageFont
    frames = directory / 'frames'
    sheets = directory / 'sheets'
    frames.mkdir(exist_ok=True)
    sheets.mkdir(exist_ok=True)
    subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-v', 'error', '-i', str(video), '-vf', 'fps=1,scale=640:-1',
                    '-start_number', '0', '-q:v', '3', '-y', str(frames / '%04d.jpg')], check=True)
    font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 17)
    files = sorted(frames.glob('*.jpg'))
    for start in range(0, len(files), 60):
        sheet = Image.new('RGB', (1920, 2040), '#111111')
        draw = ImageDraw.Draw(sheet)
        for i, path in enumerate(files[start:start + 60]):
            x, y = (i % 6) * 320, (i // 6) * 204
            frame = Image.open(path).resize((320, 180))
            sheet.paste(frame, (x, y))
            second = int(path.stem)
            draw.text((x + 5, y + 182), f'{second // 60:02}:{second % 60:02}', font=font, fill='white')
        sheet.save(sheets / f'{start // 60:02d}.jpg', quality=91)
    manifest = {'url': 'https://www.youtube.com/watch?v=i-KnKZe1L-I', 'frameIntervalSeconds': 1,
                'frameCount': len(files), 'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (video, audio)},
                'visualReview': 'pending', 'audioReview': 'pending'}
    (directory / 'evidence.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest))


if __name__ == '__main__':
    main()
