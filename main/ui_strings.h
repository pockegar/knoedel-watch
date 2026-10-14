#pragma once
// Menu and settings text, in the learner's language. tools/import_decks.py
// reads this file so that the locale fonts contain every character used here:
// keep one block per language, named as below.

#include <string.h>

struct UiText {
    const char *learn, *practice, *settings;      // main menu, under the German name
    const char *language, *language_name;         // settings: name of the item, name of this language
    const char *volume, *brightness, *power_off;  // settings
    const char *tap_change, *tap_off, *off;       // hints and values
    const char *well_done, *learn_first;          // practice: after a session, and when no card has been seen yet
    const char *clock, *timer, *time;             // main menu; settings
    const char *edit_hint;                        // settings: while a value is being changed
    const char *focus, *pause, *start_hint, *running_hint, *paused_hint;  // focus timer
    const char *break_now, *back_to_work;         // focus timer: when a phase ends
    const char *in_a_row;                         // practice: "%d" right answers in a row
    const char *weekdays[7], *months[12];         // the clock's date, Sunday first
};

inline const UiText &ui_text(const char *locale)
{
    static const UiText ZH = {"学德语", "练习", "设置", "语言", "中文", "音量", "亮度", "关机", "轻触更改", "轻触关机", "静音",
                              "做得好", "还没有卡片",
                              "时钟", "番茄钟", "时间", "按键调整 · 轻触下一项",
                              "专注", "休息", "轻触开始 · 或把手表扣过来", "轻触暂停", "轻触继续",
                              "该休息一下了", "回来继续专注", "连对 %d 个",
                              {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"},
                              {"1月", "2月", "3月", "4月", "5月", "6月", "7月", "8月", "9月", "10月", "11月", "12月"}};
    static const UiText EN = {"Learn German", "Practice", "Settings", "Language", "English", "Volume", "Brightness", "Power off",
                              "Tap to change", "Tap to switch off", "Muted",
                              "Well done", "No cards yet",
                              "Clock", "Focus timer", "Time", "Keys: adjust · Tap: next",
                              "Focus", "Break", "Tap, or turn me face down", "Tap to pause", "Tap to resume",
                              "Time for a break", "Back to focus", "%d in a row",
                              {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
                              {"January", "February", "March", "April", "May", "June", "July", "August", "September",
                               "October", "November", "December"}};
    return strncmp(locale, "zh", 2) == 0 ? ZH : EN;
}
