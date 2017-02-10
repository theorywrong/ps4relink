#include <stdio.h>
#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/types.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/message_dialog.h>
#include <psp2/ime_dialog.h>
#include <psp2/display.h>
#include <psp2/apputil.h>
#include <psp2/io/fcntl.h>
#include <psp2/power.h>
#include <psp2/ctrl.h>

#include <vita2d.h>

// Based on https://github.com/Freakler/vita-uriCaller/blob/master/main.c

#define SCE_IME_DIALOG_MAX_TITLE_LENGTH	(128)
#define SCE_IME_DIALOG_MAX_TEXT_LENGTH	(512)

#define IME_DIALOG_RESULT_NONE 0
#define IME_DIALOG_RESULT_RUNNING 1
#define IME_DIALOG_RESULT_FINISHED 2
#define IME_DIALOG_RESULT_CANCELED 3

static uint16_t ime_title_utf16[SCE_IME_DIALOG_MAX_TITLE_LENGTH];
static uint16_t ime_initial_text_utf16[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
static uint16_t ime_input_text_utf16[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
static uint8_t ime_input_text_utf8[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];

static SceCtrlData pad;
static vita2d_pvf* default_font;
static uint32_t old_buttons, current_buttons, pressed_buttons;

static int current_menu_pos = 0;
static int menu_nbr = 3;

void utf16_to_utf8(uint16_t *src, uint8_t *dst) {
	int i;
	for (i = 0; src[i]; i++) {
		if ((src[i] & 0xFF80) == 0) {
			*(dst++) = src[i] & 0xFF;
		} else if((src[i] & 0xF800) == 0) {
			*(dst++) = ((src[i] >> 6) & 0xFF) | 0xC0;
			*(dst++) = (src[i] & 0x3F) | 0x80;
		} else if((src[i] & 0xFC00) == 0xD800 && (src[i + 1] & 0xFC00) == 0xDC00) {
			*(dst++) = (((src[i] + 64) >> 8) & 0x3) | 0xF0;
			*(dst++) = (((src[i] >> 2) + 16) & 0x3F) | 0x80;
			*(dst++) = ((src[i] >> 4) & 0x30) | 0x80 | ((src[i + 1] << 2) & 0xF);
			*(dst++) = (src[i + 1] & 0x3F) | 0x80;
			i += 1;
		} else {
			*(dst++) = ((src[i] >> 12) & 0xF) | 0xE0;
			*(dst++) = ((src[i] >> 6) & 0x3F) | 0x80;
			*(dst++) = (src[i] & 0x3F) | 0x80;
		}
	}

	*dst = '\0';
}

void utf8_to_utf16(uint8_t *src, uint16_t *dst) {
	int i;
	for (i = 0; src[i];) {
		if ((src[i] & 0xE0) == 0xE0) {
			*(dst++) = ((src[i] & 0x0F) << 12) | ((src[i + 1] & 0x3F) << 6) | (src[i + 2] & 0x3F);
			i += 3;
		} else if ((src[i] & 0xC0) == 0xC0) {
			*(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
			i += 2;
		} else {
			*(dst++) = src[i];
			i += 1;
		}
	}

	*dst = '\0';
}
 
void initImeDialog(char *title, char *initial_text, int max_text_length) {
    // Convert UTF8 to UTF16
	utf8_to_utf16((uint8_t *)title, ime_title_utf16);
	utf8_to_utf16((uint8_t *)initial_text, ime_initial_text_utf16);
 
    SceImeDialogParam param;
	sceImeDialogParamInit(&param);

	param.sdkVersion = 0x03150021,
	param.supportedLanguages = 0x0001FFFF;
	param.languagesForced = SCE_TRUE;
	param.type = SCE_IME_TYPE_BASIC_LATIN;
	param.title = ime_title_utf16;
	param.maxTextLength = max_text_length;
	param.initialText = ime_initial_text_utf16;
	param.inputTextBuffer = ime_input_text_utf16;

	sceImeDialogInit(&param);
	return ;
}

void oslOskGetText(char *text){
	// Convert UTF16 to UTF8
	utf16_to_utf8(ime_input_text_utf16, ime_input_text_utf8);
	strcpy(text,(char*)ime_input_text_utf8);
}

void readPad() {
	memset(&pad, 0, sizeof(SceCtrlData));
	sceCtrlPeekBufferPositive(0, &pad, 1);
	current_buttons = pad.buttons;
	pressed_buttons = current_buttons & ~old_buttons;

	if (old_buttons != current_buttons) {
		old_buttons = current_buttons;
	}
}

void currentOnlineID(char* onlineid) {
	int fd = sceIoOpen("ur0:user/00/np/myprofile.dat", SCE_O_RDONLY, 0777);
	sceIoRead(fd, onlineid, 16);
	sceIoClose(fd);
}

void saveOnlineID(char* onlineid) {
	char data[328];
	memset(data, 0, 328);
	strcpy(data, onlineid);

	int fd = sceIoOpen("ur0:user/00/np/myprofile.dat", SCE_O_WRONLY, 0777);
	sceIoWrite(fd, data, 328);
	sceIoClose(fd);
}

int main(int argc, const char *argv[]) {
    char onlineid[17] = "yourusername";
	currentOnlineID(onlineid);

    vita2d_init();
    default_font = vita2d_load_default_pvf();

    sceCommonDialogSetConfigParam(&(SceCommonDialogConfigParam){});

    while (1) {
    	readPad();
    	if (pressed_buttons & SCE_CTRL_UP) {
    		if (current_menu_pos != 0)
    			current_menu_pos--;
    	} else if (pressed_buttons & SCE_CTRL_DOWN){
    		if (current_menu_pos != (menu_nbr-1))
				current_menu_pos++;
    	} else if (pressed_buttons & SCE_CTRL_CROSS){
    		switch (current_menu_pos) {
    			case 0: {
    				initImeDialog("Enter your Online ID", onlineid, 16);
    				break;
    			}
    			case 1: {
    				scePowerRequestColdReset();
    				break;
    			}
    			case 2: {
    				sceKernelExitProcess(0);
    				break;
    			}
    		}
    	}

	    SceCommonDialogStatus status = sceImeDialogGetStatus();
	   
		if (status == IME_DIALOG_RESULT_FINISHED) {
			SceImeDialogResult result;
			memset(&result, 0, sizeof(SceImeDialogResult));
			sceImeDialogGetResult(&result);

			if (result.button == SCE_IME_DIALOG_BUTTON_CLOSE) {
				status = IME_DIALOG_RESULT_CANCELED;
			} else {
				oslOskGetText(onlineid);
				saveOnlineID(onlineid);
			}

			sceImeDialogTerm();
		}

        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_pvf_draw_text(default_font, 10, 20, RGBA8(255, 0, 0, 255), 1.0f, "PS4 ReLink");
        vita2d_pvf_draw_textf(default_font, 10, 40, RGBA8(255, 255, 255, 255), 1.0f, "Current Online ID: %s", onlineid);
        
        if (current_menu_pos == 0) {
        	vita2d_pvf_draw_text(default_font, 10, 80, RGBA8(255, 0, 0, 255), 1.0f, ">> Change Online ID");
		} else {
        	vita2d_pvf_draw_text(default_font, 10, 80, RGBA8(255, 255, 255, 255), 1.0f, "    Change Online ID");
		}

        if (current_menu_pos == 1) {
        	vita2d_pvf_draw_text(default_font, 10, 100, RGBA8(255, 0, 0, 255), 1.0f, ">> Reboot PSVita");
		} else {
        	vita2d_pvf_draw_text(default_font, 10, 100, RGBA8(255, 255, 255, 255), 1.0f, "    Reboot PSVita");
		}

        if (current_menu_pos == 2) {
        	vita2d_pvf_draw_text(default_font, 10, 120, RGBA8(255, 0, 0, 255), 1.0f, ">> Exit application");
        } else {
        	vita2d_pvf_draw_text(default_font, 10, 120, RGBA8(255, 255, 255, 255), 1.0f, "    Exit application");
        }

       	vita2d_pvf_draw_text(default_font, 10, 160, RGBA8(255, 0, 0, 255), 1.0f, "Made by TheoryWrong");
        vita2d_pvf_draw_text(default_font, 10, 180, RGBA8(255, 0, 0, 255), 1.0f, "Thanks to @yifanlu for this method");

        vita2d_end_drawing();
        vita2d_common_dialog_update();
        vita2d_swap_buffers();
        sceDisplayWaitVblankStart();
    }

    sceKernelExitProcess(0);
    return 0;
}