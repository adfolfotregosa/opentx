/*
 * Authors (alphabetical order)
 * - Andre Bernet <bernet.andre@gmail.com>
 * - Andreas Weitl
 * - Bertrand Songis <bsongis@gmail.com>
 * - Bryan J. Rentoul (Gruvin) <gruvin@gmail.com>
 * - Cameron Weeks <th9xer@gmail.com>
 * - Erez Raviv
 * - Gabriel Birkus
 * - Jean-Pierre Parisy
 * - Karl Szmutny
 * - Michael Blandford
 * - Michal Hlavinka
 * - Pat Mackenzie
 * - Philip Moss
 * - Rob Thomson
 * - Romolo Manfredini <romolo.manfredini@gmail.com>
 * - Thomas Husterer
 *
 * opentx is based on code named
 * gruvin9x by Bryan J. Rentoul: http://code.google.com/p/gruvin9x/,
 * er9x by Erez Raviv: http://code.google.com/p/er9x/,
 * and the original (and ongoing) project by
 * Thomas Husterer, th9x: http://code.google.com/p/th9x/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "../../opentx.h"

vertpos_t s_pgOfs;
vertpos_t m_posVert;
horzpos_t m_posHorz;
int8_t s_editMode;
uint8_t s_noHi;
uint8_t calibrationState;

#if defined(AUTOSWITCH)
int8_t checkIncDecMovedSwitch(int8_t val)
{
  if (s_editMode>0) {
    int8_t swtch = getMovedSwitch();
    if (swtch) {
      div_t info = switchInfo(swtch);
      if (IS_TOGGLE(info.quot)) {
        if (info.rem != 0) {
          val = (val == swtch ? swtch+2 : swtch);
        }
      }
      else {
        val = swtch;
      }
    }
  }
  return val;
}
#endif

int8_t  checkIncDec_Ret;

INIT_STOPS(stops100, 3, -100, 0, 100)
INIT_STOPS(stops1000, 3, -1000, 0, 1000)
INIT_STOPS(stopsSwitch, 15, SWSRC_FIRST, CATEGORY_END(-SWSRC_FIRST_LOGICAL_SWITCH), CATEGORY_END(-SWSRC_FIRST_TRIM), CATEGORY_END(-SWSRC_LAST_SWITCH+1), 0, CATEGORY_END(SWSRC_LAST_SWITCH), CATEGORY_END(SWSRC_FIRST_TRIM-1), CATEGORY_END(SWSRC_FIRST_LOGICAL_SWITCH-1), SWSRC_LAST)

int checkIncDec(unsigned int event, int val, int i_min, int i_max, unsigned int i_flags, IsValueAvailable isValueAvailable, const CheckIncDecStops &stops)
{
  int newval = val;

#if defined(DBLKEYS)
  unsigned int in = KEYS_PRESSED();
  if (!(i_flags & NO_DBLKEYS) && (EVT_KEY_MASK(event))) {
    bool dblkey = true;
    if (DBLKEYS_PRESSED_RGT_LFT(in)) {
      if (!isValueAvailable || isValueAvailable(-val))
        newval = -val;
    }
    else if (DBLKEYS_PRESSED_RGT_UP(in)) {
      newval = (i_max > stops.max() ? stops.max() : i_max);
      while (isValueAvailable && !isValueAvailable(newval) && newval>i_min) {
        --newval;
      }
    }
    else if (DBLKEYS_PRESSED_LFT_DWN(in)) {
      newval = (i_min < stops.min() ? stops.min() : i_min);
      while (isValueAvailable && !isValueAvailable(newval) && newval<i_max) {
        ++newval;
      }
    }
    else if (DBLKEYS_PRESSED_UP_DWN(in)) {
      newval = 0;
    }
    else {
      dblkey = false;
    }

    if (dblkey) {
      killEvents(KEY_UP);
      killEvents(KEY_DOWN);
      killEvents(KEY_RIGHT);
      killEvents(KEY_LEFT);
      killEvents(KEY_PAGE);
      killEvents(KEY_MENU);
      killEvents(KEY_ENTER);
      killEvents(KEY_EXIT);
      event = 0;
    }
  }
#endif

  if (s_editMode>0 && (IS_ROTARY_RIGHT(event) || event==EVT_KEY_FIRST(KEY_UP) || event==EVT_KEY_REPT(KEY_UP))) {
    do {
      if (IS_KEY_REPT(event) && (i_flags & INCDEC_REP10)) {
        newval += min(10, i_max-val);
      }
      else {
        newval++;
      }
    } while (isValueAvailable && !isValueAvailable(newval) && newval<=i_max);

    if (newval > i_max) {
      newval = val;
      killEvents(event);
      AUDIO_WARNING2();
    }
    else {
      AUDIO_KEYPAD_UP();
    }
  }
  else if (s_editMode>0 && (IS_ROTARY_LEFT(event) || event==EVT_KEY_FIRST(KEY_DOWN) || event==EVT_KEY_REPT(KEY_DOWN))) {
    do {
      if (IS_KEY_REPT(event) && (i_flags & INCDEC_REP10)) {
        newval -= min(10, val-i_min);
      }
      else {
        newval--;
      }
    } while (isValueAvailable && !isValueAvailable(newval) && newval>=i_min);

    if (newval < i_min) {
      newval = val;
      killEvents(event);
      AUDIO_WARNING2();
    }
    else {
      AUDIO_KEYPAD_DOWN();
    }
  }

  if (!READ_ONLY() && i_min==0 && i_max==1 && (event==EVT_KEY_BREAK(KEY_ENTER) || IS_ROTARY_BREAK(event))) {
    s_editMode = 0;
    newval = !val;
  }

#if defined(AUTOSWITCH)
  if (i_flags & INCDEC_SWITCH) {
    newval = checkIncDecMovedSwitch(newval);
  }
#endif

#if defined(AUTOSOURCE)
  if (i_flags & INCDEC_SOURCE) {
    if (s_editMode>0) {
      int source = GET_MOVED_SOURCE(i_min, i_max);
      if (source) {
        newval = source;
      }
#if defined(AUTOSWITCH)
      else {
        unsigned int swtch = abs(getMovedSwitch());
        if (swtch) {
          newval = switchToMix(swtch);
        }
      }
#endif
    }
  }
#endif

  if (newval > i_max || newval < i_min) {
    newval = (newval > i_max ? i_max : i_min);
    killEvents(event);
    AUDIO_WARNING2();
  }

  if (newval != val) {
    if (!(i_flags & NO_INCDEC_MARKS) && (newval != i_max) && (newval != i_min) && stops.contains(newval) && !IS_ROTARY_EVENT(event)) {
      bool pause = (newval > val ? !stops.contains(newval+1) : !stops.contains(newval-1));
      if (pause) {
        pauseEvents(event); // delay before auto-repeat continues
        if (newval>val) // without AUDIO it's optimized, because the 2 sounds are the same
          AUDIO_KEYPAD_UP();
        else
          AUDIO_KEYPAD_DOWN();
      }
    }
    eeDirty(i_flags & (EE_GENERAL|EE_MODEL));
    checkIncDec_Ret = (newval > val ? 1 : -1);
  }
  else {
    checkIncDec_Ret = 0;
  }
  return newval;
}

#define CURSOR_NOT_ALLOWED_IN_ROW(row)   ((int8_t)MAXCOL(row) < 0)
#define MAXCOL_RAW(row) (horTab ? pgm_read_byte(horTab+min(row, (vertpos_t)horTabMax)) : (const uint8_t)0)
#define MAXCOL(row)     (MAXCOL_RAW(row) >= HIDDEN_ROW ? MAXCOL_RAW(row) : (const uint8_t)(MAXCOL_RAW(row) & (~NAVIGATION_LINE_BY_LINE)))
#define COLATTR(row)    (MAXCOL_RAW(row) == (uint8_t)-1 ? (const uint8_t)0 : (const uint8_t)(MAXCOL_RAW(row) & NAVIGATION_LINE_BY_LINE))
#define INC(val, min, max) if (val<max) {val++;} else {val=min;}
#define DEC(val, min, max) if (val>min) {val--;} else {val=max;}

coord_t scrollbar_X = DEFAULT_SCROLLBAR_X;

void onLongMenuPress(const char *result)
{
  if (result == STR_VIEW_CHANNELS) {
    pushMenu(menuChannelsView);
  }
  else if (result == STR_VIEW_NOTES) {
    pushModelNotes();
  }
}

tmr10ms_t menuEntryTime;

void check(check_event_t event, uint8_t curr, const MenuFuncP *menuTab, uint8_t menuTabSize, const pm_uint8_t *horTab, uint8_t horTabMax, vertpos_t maxrow, uint8_t flags)
{
  vertpos_t l_posVert = m_posVert;
  horzpos_t l_posHorz = m_posHorz;

  uint8_t maxcol = MAXCOL(l_posVert);

  if (menuTab) {
    int8_t cc = curr;
    switch (event) {
      case EVT_KEY_LONG(KEY_MENU):
        if (menuTab == menuTabModel) {
          killEvents(event);
          if (modelHasNotes()) {
            MENU_ADD_SD_ITEM(STR_VIEW_CHANNELS);
            MENU_ADD_ITEM(STR_VIEW_NOTES);
            menuHandler = onLongMenuPress;
          }
          else {
            pushMenu(menuChannelsView);
          }
        }
        break;

      case EVT_KEY_LONG(KEY_PAGE):
        if (curr > 0)
          cc = curr - 1;
        else
          cc = menuTabSize-1;
        killEvents(event);
        break;

      case EVT_KEY_BREAK(KEY_PAGE):
        if (curr < (menuTabSize-1))
          cc = curr + 1;
        else
          cc = 0;
        break;
    }

    if (!calibrationState && cc != curr) {
      chainMenu((MenuFuncP)pgm_read_adr(&menuTab[cc]));
    }

    if (!(flags&CHECK_FLAG_NO_SCREEN_INDEX)) {
      displayScreenIndex(curr, menuTabSize, 0);
    }

    drawFilledRect(0, 0, LCD_W, MENU_TITLE_HEIGHT, SOLID, FILL_WHITE|GREY_DEFAULT);
  }

  DISPLAY_PROGRESS_BAR(menuTab ? lcdLastPos-2*FW-((curr+1)/10*FWNUM)-2 : 20*FW+1);

  switch(event)
  {
    case EVT_ENTRY:
      menuEntryTime = get_tmr10ms();
      l_posVert = POS_VERT_INIT;
      l_posHorz = POS_HORZ_INIT(l_posVert);
      SET_SCROLLBAR_X(LCD_W-1);
      s_editMode = EDIT_MODE_INIT;
      break;

    case EVT_ENTRY_UP:
      menuEntryTime = get_tmr10ms();
      s_editMode = 0;
      l_posHorz = POS_HORZ_INIT(l_posVert);
      SET_SCROLLBAR_X(LCD_W-1);
      break;

    case EVT_ROTARY_BREAK:
      if (s_editMode > 1) break;
      if (m_posHorz < 0 && maxcol > 0 && READ_ONLY_UNLOCKED()) {
        l_posHorz = 0;
        break;
      }
      if (!menuTab || l_posVert>0) {
        if (READ_ONLY_UNLOCKED()) {
          s_editMode = (s_editMode<=0);
        }
      }
      break;

    case EVT_KEY_LONG(KEY_EXIT):
      s_editMode = 0; // TODO needed? we call ENTRY_UP after which does the same
      popMenu();
      break;

    case EVT_KEY_BREAK(KEY_EXIT):
      if (s_editMode>0) {
        s_editMode = 0;
        break;
      }

      if (l_posHorz >= 0 && (COLATTR(l_posVert) & NAVIGATION_LINE_BY_LINE)) {
        l_posHorz = -1;
      }
      else
      {
        uint8_t posVertInit = POS_VERT_INIT;
        if (s_pgOfs != 0 || l_posVert != posVertInit) {
          s_pgOfs = 0;
          l_posVert = posVertInit;
          l_posHorz = POS_HORZ_INIT(l_posVert);
        }
        else {
          popMenu();
        }
      }
      break;

    CASE_EVT_ROTARY_MOVE_RIGHT
      if (s_editMode != 0) break;
      if ((COLATTR(l_posVert) & NAVIGATION_LINE_BY_LINE)) {
        if (l_posHorz >= 0) {
          INC(l_posHorz, 0, maxcol);
          break;
        }
      }
      else {
        if (l_posHorz < maxcol) {
          l_posHorz++;
          break;
        }
        else {
          l_posHorz = 0;
          if (!IS_ROTARY_MOVE_RIGHT(event))
            break;
        }
      }

      do {
        INC(l_posVert, POS_VERT_INIT, maxrow);
      } while (CURSOR_NOT_ALLOWED_IN_ROW(l_posVert));

      s_editMode = 0; // if we go down, we must be in this mode

      l_posHorz = POS_HORZ_INIT(l_posVert);
      break;

    CASE_EVT_ROTARY_MOVE_LEFT
      if (s_editMode != 0) break;
      if ((COLATTR(l_posVert) & NAVIGATION_LINE_BY_LINE)) {
        if (l_posHorz >= 0) {
          DEC(l_posHorz, 0, maxcol);
          break;
        }
      }
      else {
        if (l_posHorz > 0) {
          l_posHorz--;
          break;
        }
        else if (IS_ROTARY_MOVE_LEFT(event) && s_editMode == 0) {
          l_posHorz = 0xff;
        }
        else {
          l_posHorz = maxcol;
          break;
        }
      }

      do {
        DEC(l_posVert, POS_VERT_INIT, maxrow);
      } while (CURSOR_NOT_ALLOWED_IN_ROW(l_posVert));

      s_editMode = 0; // if we go up, we must be in this mode

      if ((COLATTR(l_posVert) & NAVIGATION_LINE_BY_LINE))
        l_posHorz = -1;
      else
        l_posHorz = min((uint8_t)l_posHorz, MAXCOL(l_posVert));

      break;
  }

  if (l_posVert == 0 || (l_posVert==2 && MAXCOL(vertpos_t(1)) >= HIDDEN_ROW) || (l_posVert==3 && MAXCOL(vertpos_t(1)) >= HIDDEN_ROW && MAXCOL(vertpos_t(2)) >= HIDDEN_ROW)) {
    s_pgOfs = 0;
  }
  else if (horTab) {
    uint8_t max = menuTab ? NUM_BODY_LINES : NUM_BODY_LINES-1;
    if (maxrow > max) {
      while (1) {
        vertpos_t firstLine = 0;
        for (int numLines=0; firstLine<=maxrow && numLines<s_pgOfs; firstLine++) {
          if (horTab[firstLine+1] != HIDDEN_ROW) {
            numLines++;
          }
        }
        if (l_posVert <= firstLine) {
          s_pgOfs--;
        }
        else {
          vertpos_t lastLine = firstLine;
          for (int numLines=0; lastLine<=maxrow && numLines<max; lastLine++) {
            if (horTab[lastLine+1] != HIDDEN_ROW) {
              numLines++;
            }
          }
          if (l_posVert > lastLine) {
            s_pgOfs++;
          }
          else {
            break;
          }
        }
      }
    }
  }
  else {
    uint8_t max = menuTab ? NUM_BODY_LINES : NUM_BODY_LINES-1;
    if (l_posVert>max+s_pgOfs) {
      s_pgOfs = l_posVert-max;
    }
    else if (l_posVert<1+s_pgOfs) {
      s_pgOfs = l_posVert-1;
    }
  }

  if (maxrow > NUM_BODY_LINES && scrollbar_X) {
    displayScrollbar(scrollbar_X, MENU_TITLE_HEIGHT, LCD_H-MENU_TITLE_HEIGHT-MENU_NAVIG_HEIGHT, s_pgOfs, menuTab ? maxrow : maxrow+1, NUM_BODY_LINES);
  }

  m_posVert = l_posVert;
  m_posHorz = l_posHorz;
}


void check_simple(check_event_t event, uint8_t curr, const MenuFuncP *menuTab, uint8_t menuTabSize, vertpos_t maxrow)
{
  check(event, curr, menuTab, menuTabSize, 0, 0, maxrow);
}

void check_submenu_simple(check_event_t event, uint8_t maxrow)
{
  check_simple(event, 0, 0, 0, maxrow);
}

void repeatLastCursorMove(uint8_t event)
{
  if (CURSOR_MOVED_LEFT(event) || CURSOR_MOVED_RIGHT(event)) {
    putEvent(event);
  }
  else {
    m_posHorz = 0;
  }
}
