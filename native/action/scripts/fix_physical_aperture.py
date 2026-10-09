"""Create the local pinned-runtime override; leave the toolkit checkout intact."""
from pathlib import Path
import hashlib
root=Path(__file__).resolve().parents[1]
source=root.parent/'xboxrecomp/src/kernel/xbox_memory_layout.c'
assert hashlib.sha256(source.read_bytes()).hexdigest()=='7fa159eeff28c018104cdaf59a5c82c422eb408ed09b43c322a0b74bf4c46cfb'
s=source.read_text()
def replace(a,b):
    global s
    assert s.count(a)==1, a
    s=s.replace(a,b)
replace('static void *g_contig_memory = NULL;', 'static void *g_contig_memory = NULL;\nstatic HANDLE g_contig_mapping = NULL; /* Nightfire: shared physical aperture */')
replace('''        g_contig_memory = VirtualAlloc(
            (LPVOID)contig_native,
            XBOX_CONTIG_SIZE,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE
        );''','''        /* PAL LockRect ORs F0000000 into the physical texture address.
         * Its write-combined view must share contiguous allocations, not the
         * independently backed low image/heap arena. */
        g_contig_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
            PAGE_READWRITE, 0, XBOX_CONTIG_SIZE, NULL);
        g_contig_memory = g_contig_mapping ? MapViewOfFileEx(g_contig_mapping,
            FILE_MAP_ALL_ACCESS, 0, 0, XBOX_CONTIG_SIZE,
            (LPVOID)contig_native) : NULL;''')
replace('''        g_tiled_view = MapViewOfFileEx(
            g_mapping_handle,''','''        g_tiled_view = MapViewOfFileEx(
            g_contig_mapping,''')
replace('''                    XBOX_CONTIG_BASE, GetLastError());
        }
    }''','''                    XBOX_CONTIG_BASE, GetLastError());
            return 0;
        }
    }''')
replace('''                    XBOX_TILED_BASE, GetLastError());
        }
    }''','''                    XBOX_TILED_BASE, GetLastError());
            return 0;
        }
    }''')
replace('(volatile uint32_t *)((uintptr_t)0x1000 + g_memory_offset);','(volatile uint32_t *)((uintptr_t)(XBOX_CONTIG_BASE + 0x1000) + g_memory_offset);')
replace('''                uint32_t saved = *via_ram;

                *via_tiled''','''                volatile uint32_t *low = (volatile uint32_t *)(g_memory_offset + 0x1000);
                uint32_t saved = *via_ram, saved_low = *low;

                *via_tiled''')
replace('''                *via_ram = saved;
            }
            fprintf(stderr, "  Tiled aperture:''','''                int valid = *via_ram == 0xA5C30F17u && *low == saved_low;
                *via_ram = 0x13579BDFu;
                valid = valid && *via_tiled == 0x13579BDFu && *low == saved_low;
                *via_ram = saved;
                if (!valid) { fprintf(stderr,"[PHYSICAL-ALIAS] FAILED\\n"); return 0; }
                fprintf(stderr,"[PHYSICAL-ALIAS] PASS: bidirectional tiled/contiguous sharing, low heap unchanged\\n");
            }
            fprintf(stderr, "  Tiled aperture:''')
replace('''        VirtualFree(g_kernel_memory, 0, MEM_RELEASE);
        g_kernel_memory = NULL;''','''        if (!g_contig_memory) VirtualFree(g_kernel_memory, 0, MEM_RELEASE);
        g_kernel_memory = NULL;''')
replace('''    /* Unmap base view */''','''    if (g_tiled_view) { UnmapViewOfFile(g_tiled_view); g_tiled_view = NULL; }
    if (g_contig_memory) { UnmapViewOfFile(g_contig_memory); g_contig_memory = NULL; }
    if (g_contig_mapping) { CloseHandle(g_contig_mapping); g_contig_mapping = NULL; }
    /* Unmap base view */''')
(root/'runtime/xbox_memory_layout.c').write_text('/* Checkpoint 26 local override of pinned xboxrecomp memory layout. */\n'+s)
