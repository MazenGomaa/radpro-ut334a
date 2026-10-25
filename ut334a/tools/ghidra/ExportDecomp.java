// Export decompiled C for every function, plus a function list with callers and strings.
// Run as -postScript with the output directory as the first argument.
// @category FM33

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.StringDataInstance;
import java.io.*;
import java.util.*;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]);
        out.mkdirs();
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try (PrintWriter c = new PrintWriter(new File(out, "decompiled.c"), "UTF-8");
             PrintWriter l = new PrintWriter(new File(out, "functions.txt"), "UTF-8")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                Set<String> callers = new TreeSet<>();
                for (Function g : f.getCallingFunctions(monitor)) callers.add(g.getName());
                l.printf("%s %-36s size=%d callers=%s%n", f.getEntryPoint(), f.getName(),
                         f.getBody().getNumAddresses(), callers);
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                c.printf("// ===== %s @ %s =====%n", f.getName(), f.getEntryPoint());
                c.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
            }
        }
        try (PrintWriter s = new PrintWriter(new File(out, "strings.txt"), "UTF-8")) {
            for (Data dt : currentProgram.getListing().getDefinedData(true)) {
                if (dt.hasStringValue()) s.printf("%s %s%n", dt.getAddress(), StringDataInstance.getStringDataInstance(dt).getStringValue());
            }
        }
        d.dispose();
        println("Exported to " + out);
    }
}
