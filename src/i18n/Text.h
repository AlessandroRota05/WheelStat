#pragma once

/*
  WheelStat - src/i18n/Text.h
  ===========================================================================
  Every string the rider sees, on the OLED and on the website. The language
  is fixed at compile time in Config.h (LANG_IT / LANG_EN): only one of the
  two tables below is compiled, so a second language costs no flash.

  The strings are macros, not variables, so they can be glued into larger
  literals at compile time:  page.print("<h2>" TR_W_RECORDS "</h2>");

  Rules for a translation (they apply to a new language file too):
    TR_O_*  OLED. Plain ASCII only (the built-in font has no accented
            letters) and mind the width: 21 characters per line, and the
            labels of a data row must fit in 8. Keep any trailing spaces.
    TR_W_*  Website, UTF-8. No ' or " characters: several strings end up
            inside HTML attributes or JavaScript literals. Use the
            typographic apostrophe (’) instead.
    TR_E_*  Plain-text error replies of the website, UTF-8.
    TR_CH_* Channel names for the website tables.

  To add a language: copy Text_en.h, translate it, add a LANG_xx macro in
  Config.h and a branch here. Both existing tables must define the same
  keys - building once per language is the check.
  ===========================================================================
*/

#include "../../Config.h"

#if defined(LANG_EN)
  #include "Text_en.h"
#elif defined(LANG_IT)
  #include "Text_it.h"
#else
  #error "Config.h: no language selected (LANG_IT or LANG_EN)"
#endif
