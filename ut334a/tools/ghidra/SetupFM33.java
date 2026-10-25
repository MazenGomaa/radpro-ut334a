// Prepare a raw FM33LC0xx flash image (loaded at 0x0, ARM:LE:32:Cortex) for analysis:
// RAM/peripheral/SCS memory blocks, SVD register labels, Thumb mode, vector-table entry points,
// and names established by hand. Run as -preScript with the SVD path as the first argument.
// @category FM33

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.data.*;
import org.w3c.dom.*;
import javax.xml.parsers.DocumentBuilderFactory;
import java.io.File;
import java.math.BigInteger;
import java.util.*;

public class SetupFM33 extends GhidraScript {

    private long num(String s) {
        s = s.trim().toLowerCase();
        return s.startsWith("0x") ? Long.parseLong(s.substring(2), 16) : Long.parseLong(s);
    }

    private String text(Element e, String tag) {
        NodeList l = e.getElementsByTagName(tag);
        for (int i = 0; i < l.getLength(); i++) {
            if (l.item(i).getParentNode() == e) return l.item(i).getTextContent().trim();
        }
        return null;
    }

    private void block(String name, long start, long size, boolean volatil) throws Exception {
        Memory mem = currentProgram.getMemory();
        Address a = toAddr(start);
        if (mem.getBlock(a) != null) return;
        MemoryBlock b = mem.createUninitializedBlock(name, a, size, false);
        b.setRead(true); b.setWrite(true); b.setExecute(false); b.setVolatile(volatil);
    }

    private void label(long addr, String name) throws Exception {
        createLabel(toAddr(addr), name, true, SourceType.USER_DEFINED);
    }

    private void func(long addr, String name) throws Exception {
        Address a = toAddr(addr & ~1L);
        if (getInstructionAt(a) == null) {
            setThumb(a);
            disassemble(a);
        }
        Function f = getFunctionAt(a);
        if (f == null) f = createFunction(a, name);
        else f.setName(name, SourceType.USER_DEFINED);
    }

    private void setThumb(Address a) throws Exception {
        Register t = currentProgram.getProgramContext().getRegister("TMode");
        currentProgram.getProgramContext().setValue(t, a, a, BigInteger.ONE);
    }

    @Override
    public void run() throws Exception {
        String svd = getScriptArgs().length > 0 ? getScriptArgs()[0] : askFile("SVD", "Use").getPath();

        // Whole flash is Thumb code/data
        Register t = currentProgram.getProgramContext().getRegister("TMode");
        currentProgram.getProgramContext().setValue(t, toAddr(0), toAddr(0x3FFFF), BigInteger.ONE);

        block("RAM", 0x20000000L, 0x6000, false);
        block("INFO", 0x1FFFFA00L, 0x600, false);
        block("SCS", 0xE000E000L, 0x1000, true);

        // Peripheral blocks + register labels from the SVD
        Element root = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(new File(svd)).getDocumentElement();
        NodeList ps = root.getElementsByTagName("peripheral");
        Map<String, Element> byName = new HashMap<>();
        for (int i = 0; i < ps.getLength(); i++) {
            Element p = (Element) ps.item(i);
            String n = text(p, "name");
            if (n != null && !n.isEmpty()) byName.put(n, p);
        }
        for (Element p : byName.values()) {
            String pname = text(p, "name");
            String baseS = text(p, "baseAddress");
            if (baseS == null || baseS.isEmpty()) continue;
            long base = num(baseS);
            Element src = p.hasAttribute("derivedFrom") ? byName.get(p.getAttribute("derivedFrom").trim()) : p;
            long size = 0x40;
            NodeList ab = src.getElementsByTagName("addressBlock");
            if (ab.getLength() > 0) size = Math.max(4, num(text((Element) ab.item(0), "size")));
            try { block(pname, base, size, true); } catch (Exception e) { /* overlapping blocks: labels still work */ }
            NodeList regs = src.getElementsByTagName("register");
            for (int j = 0; j < regs.getLength(); j++) {
                Element r = (Element) regs.item(j);
                long off = num(text(r, "addressOffset"));
                try {
                    label(base + off, pname + "_" + text(r, "name"));
                    Address ra = toAddr(base + off);
                    if (getDataAt(ra) == null) createData(ra, DWordDataType.dataType);
                } catch (Exception e) { /* address outside any block */ }
            }
        }
        String[][] scs = {{"E000E010","SysTick_CTRL"},{"E000E014","SysTick_LOAD"},{"E000E018","SysTick_VAL"},
            {"E000E100","NVIC_ISER"},{"E000E180","NVIC_ICER"},{"E000E200","NVIC_ISPR"},{"E000E280","NVIC_ICPR"},
            {"E000ED00","SCB_CPUID"},{"E000ED04","SCB_ICSR"},{"E000ED0C","SCB_AIRCR"},{"E000ED10","SCB_SCR"}};
        for (String[] s : scs) label(Long.parseLong(s[0], 16), s[1]);

        // Vector table
        String[] vec = {"Reset_Handler","NMI_Handler","HardFault_Handler"};
        for (int i = 1; i < 48; i++) {
            long v = getInt(toAddr(i * 4)) & 0xFFFFFFFFL;
            createData(toAddr(i * 4), PointerDataType.dataType);
            if (v == 0 || v >= 0x40000) continue;
            String n = i <= 3 ? vec[i - 1] : (i >= 16 ? "IRQ" + (i - 16) + "_Handler" : "Exc" + i + "_Handler");
            try { func(v, n); } catch (Exception e) { }
        }
        createData(toAddr(0), PointerDataType.dataType);

        // Names established by hand (see docs/ut334a-notes.md)
        Object[][] names = {
            {0xC6FCL, "SystemInit"}, {0xC0L, "__main"}, {0xDC68L, "main"},
            {0x2750L, "COMP_IRQHandler_gm_pulse"}, {0x6254L, "LPTIM_IRQHandler_tick"},
            {0x7830L, "RTC_IRQHandler"}, {0x4F40L, "GPIO_IRQHandler"},
            {0x4128L, "FL_GPIO_Init"}, {0x6A90L, "board_init"}, {0x6B64L, "board_gpio_init"},
            {0x6950L, "adc_init"}, {0x69C4L, "atim_backlight_pwm_init"}, {0x6A1CL, "atim_timebase_init"},
            {0x6A4CL, "comp2_gm_init"}, {0x6C6CL, "gptim0_tone_pwm_init"}, {0x6CCCL, "gptim0_timebase_init"},
            {0x6D18L, "lptim_init"}, {0x6CF8L, "lptim_enable_irq"}, {0x6D48L, "nvic_init"},
            {0x6B34L, "dma_init"}, {0x6D70L, "pmu_init"}, {0x6CB4L, "gptim0_set_ccr1"},
            {0xA884L, "power_off_deepsleep"}, {0x58B8L, "screen_wake"}, {0x5EB8L, "screen_sleep"},
            {0x5960L, "lcd_init"}, {0xBA90L, "spi1_init"}, {0xBB04L, "spi1_dma_send"},
            {0x6208L, "lcd_cmd"}, {0x61D4L, "lcd_data8"}, {0x61A0L, "lcd_data16"},
            {0x5C04L, "lcd_set_window"}, {0x5C74L, "lcd_fill_rect"}, {0x3EA8L, "delay_ms"},
            {0x23FAL, "__decompress"}, {0x2484L, "poll_button_pa15"},
        };
        for (Object[] n : names) {
            try { func((Long) n[0], (String) n[1]); } catch (Exception e) { println("name failed " + n[1] + ": " + e); }
        }
        label(0x20000E34L, "g_pulse_count");
        label(0xE548L, "font_8x16");
        label(0xF96EL, "font_big_digits");
        label(0xEB38L, "font_cn16_table");
    }
}
