// Offline recovery of selected functions from the user-supplied Dawn release.
// @category Dawn
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.nio.file.*;

public class RecoverWaypoints extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        Path output = Path.of(args[0]);
        Files.createDirectories(output);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long image = currentProgram.getImageBase().getOffset();
        for (int n = 1; n < args.length; ++n) {
            String[] range = args[n].split(":");
            long lo = Long.parseUnsignedLong(range[0], 16);
            long hi = range.length > 1 ? Long.parseUnsignedLong(range[1], 16) : lo + 1;
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(toAddr(image + lo), true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function f = functions.next();
                long rva = f.getEntryPoint().getOffset() - image;
                if (rva >= hi) break;
                DecompileResults result = decompiler.decompileFunction(f, 45, monitor);
                String text = "// RVA " + Long.toHexString(rva) + " " + f.getName() + "\n";
                if (result.decompileCompleted()) text += result.getDecompiledFunction().getC();
                else text += "// ERROR: " + result.getErrorMessage();
                Files.writeString(output.resolve(Long.toHexString(rva) + ".c"), text);
                println("Recovered " + Long.toHexString(rva) + " " + f.getName());
            }
        }
        decompiler.dispose();
    }
}
