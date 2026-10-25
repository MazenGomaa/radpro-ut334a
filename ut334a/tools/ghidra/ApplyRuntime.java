// Name and type the Keil/ARM EABI runtime helpers identified by emulation (tools/emu.py),
// so the decompiler shows float/double maths. Run as -postScript before ExportDecomp.java.
// @category FM33

import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParserUtils;
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class ApplyRuntime extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[][] rt = {
            {"1264", "double __aeabi_f2d(float a)"},
            {"6F4",  "float __aeabi_d2f(double a)"},
            {"1490", "float __aeabi_i2f(int a)"},
            {"14A0", "float __aeabi_ui2f(unsigned int a)"},
            {"1016", "double __aeabi_ui2d(unsigned int a)"},
            {"E22C", "float __aeabi_fadd(float a, float b)"},
            {"E360", "float __aeabi_fmul(float a, float b)"},
            {"12B8", "float __aeabi_fdiv(float a, float b)"},
            {"101C", "double __aeabi_dmul(double a, double b)"},
            {"AC8",  "double __aeabi_ddiv(double a, double b)"},
            {"E160", "void __aeabi_cdcmple(double a, double b)"},
            {"E2B8", "void __aeabi_cfcmple(float a, float b)"},
            {"E410", "void __aeabi_cfrcmple(float a, float b)"},
        };
        for (String[] r : rt) {
            long a = Long.parseLong(r[0], 16);
            Function f = getFunctionAt(toAddr(a));
            if (f == null) f = createFunction(toAddr(a), null);
            FunctionDefinitionDataType sig = CParserUtils.parseSignature(
                (ghidra.app.services.DataTypeManagerService) null, currentProgram, r[1]);
            new ApplyFunctionSignatureCmd(f.getEntryPoint(), sig, SourceType.USER_DEFINED).applyTo(currentProgram);
            println("applied " + r[1]);
        }
        String[][] names = {
            {"5008", "update_calibration_factor"}, {"5094", "measurement_tick"}, {"284C", "menu_data_draw"},
            {"8734", "dose_screen_draw"},
        };
        for (String[] n : names) {
            Function f = getFunctionAt(toAddr(Long.parseLong(n[0], 16)));
            if (f != null) f.setName(n[1], SourceType.USER_DEFINED);
        }
    }
}
