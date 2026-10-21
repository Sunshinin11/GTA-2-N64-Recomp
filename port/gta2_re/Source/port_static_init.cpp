// Browser port: run-time replacements for the original executable's static
// initialisers (its C runtime start-up, _cinit, built these tables before
// WinMain). Upstream's standalone build only runs them from the ImGui debug
// "boot to map" path (ImGuiDebug.cpp, excluded here), so the port calls them
// once at start-up instead. The two small ones are copied from ImGuiDebug.cpp.
#include "CarInfo_808.hpp"
#include "InputRecorder_67F8B0.hpp"
#include "ObjectDefinitions_8CA8.hpp"
#include "fix16.hpp"
#include "map_0x370.hpp"
#include "sprite.hpp"

void Init_gmp_slopes_array();
void Init_trigonometry_tables();

// crt_init_own_libname_21 or sub_4F7530
static void Init_Unk_Width_Height_F16_array()
{
    for (u32 i = 0; i < GTA2_COUNTOF(gPixelsToFix16_6F6850.list); i++)
    {
        gPixelsToFix16_6F6850.list[i] = Fix16(i) / 64;
    }
}

// InputRecorder_67F8B0 *crt_init_4CDCB0
static void Init_InputRecorder()
{
    gInputRecorder_67F8B0.field_0_bShutDown = false;
    gInputRecorder_67F8B0.field_4_input_bits = 0;
    gInputRecorder_67F8B0.field_75344_bInputEnabled = true;
    gInputRecorder_67F8B0.field_38_replay_state = Disabled_2;
    gInputRecorder_67F8B0.field_75345_attract_idx = 0;
    memset(gInputRecorder_67F8B0.field_8_input_masks, 0, sizeof(gInputRecorder_67F8B0.field_8_input_masks));
    memset(gInputRecorder_67F8B0.field_3C_rec_buff, 0, sizeof(gInputRecorder_67F8B0.field_3C_rec_buff));
    gInputRecorder_67F8B0.field_7533C_used_recs_count = 205;
    gInputRecorder_67F8B0.field_75340_rec_buf_idx = 205;
    gInputRecorder_67F8B0.VerifyAttractFilesExist_4CE650();
}

extern "C" void Gta2Port_StaticInit()
{
    InitMapObjectOverrides();
    InitCodeObjectTemplates();
    Init_gmp_slopes_array();
    Init_trigonometry_tables();
    Init_Unk_Width_Height_F16_array();
    Init_InputRecorder();
}
