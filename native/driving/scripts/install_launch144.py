"""Install optional original binary launch-page provider after prepare_runtime."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'runtime/kernel_bridge.c';text=p.read_text(encoding='utf-8')
decl='extern uint32_t driving_launch_page144(void);\n'
anchor='static void kernel_data_init(void)'
assert text.count(anchor)==1
if decl not in text:text=text.replace(anchor,decl+anchor)
old='        const char *cmdline = getenv("RECOMP_CMDLINE");\n\n        if (cmdline && *cmdline) {'
new='''        const char *cmdline = getenv("RECOMP_CMDLINE");
        uint32_t driving_page = driving_launch_page144();
        if (driving_page) {
            BRIDGE_MEM32(XBOX_KERNEL_DATA_BASE + KDATA_LAUNCH_DATA_PAGE) = driving_page;
        } else if (cmdline && *cmdline) {'''
if new not in text:
    assert text.count(old)==1
    text=text.replace(old,new)
p.write_text(text,encoding='utf-8')
print('Installed optional full binary launch page; null fallback unchanged when no payload selected')
