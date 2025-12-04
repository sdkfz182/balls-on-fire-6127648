#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <panel.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cJSON.h"

// LAST COMMIT: 11-22-2025
// 11-10-2025

/*
 
v0.3 
  - Refactored code
v0.2 
 - Smooth esc
 - Popup when saving
 - Binded item delted to 'd'
 - Mark/Unmark as complete with 'm'
*/

void placeholderVoid () {
  
}

typedef struct {
  char *title;
  void (*run)(void);
} ApplicationItem;

typedef struct TodoItem {
  bool completed;
  char *name;
  char *description;
  struct TodoItem *next;
} TodoItem;

typedef struct TodoGroup {
  char *name;
  TodoItem *todoHead;
  TodoItem *todoTail;
  struct TodoGroup *nextGroup;
  bool collapsed;
} TodoGroup;

typedef struct TodoPage {
  char *name;
  struct TodoPage *nextPage;
  TodoGroup *groupHead;
  TodoGroup *groupTail;
} TodoPage;

typedef enum {
  TODO,
  MENU,
  ADD,
  PAGE_SELECT,
  MOVE,
} todoMode;

TodoItem *createTodo(char *_title, bool _completed, char* _description) {
  char* newStr = (char*)malloc(strlen(_title) * 2);
  strcpy(newStr, _title);
  TodoItem* newTodo = (TodoItem*)malloc(sizeof(TodoItem));
  newTodo->completed = _completed;
  newTodo->name = newStr;
  newTodo->description = _description;
  newTodo->next = NULL;
  return newTodo;
}

void addTodo(TodoGroup** group, char* _title, bool _completed, char* _description) {
  char *new_title = _title;
  TodoItem *newItem = createTodo(new_title, _completed, _description);
  TodoGroup *_group = *group;
  if(_group->todoHead == NULL) {
    _group->todoHead = newItem;
    _group->todoTail = newItem;
    return;
  }
  
  if(_group->todoTail != NULL) {
    _group->todoTail->next = newItem;
    _group->todoTail = _group->todoTail->next;
  }
}

void addTodoFromHead(TodoGroup** group, char* _title, bool _completed, char* _description) {
  char *new_title = _title;
  TodoItem *newItem = createTodo(new_title, _completed, _description);
  TodoGroup *_group = *group;

  if(_group->todoHead == NULL) {
    _group->todoHead = newItem;
    _group->todoTail = newItem;
  }
  else {
    newItem->next = _group->todoHead;
    _group->todoHead = newItem;
  }
}

void deleteTodo(TodoItem *todoDel, TodoGroup** group) {
  TodoGroup *_group = *group; 
  TodoItem *temp;
  TodoItem *prev;

  if(_group->todoHead != NULL) { temp = _group->todoHead; }
  else { return; }
  if(temp == todoDel) {
    _group->todoHead = temp->next;
    free(temp->name);
    free(temp);
    return;
  }

  while(temp->next != NULL) {
    prev = temp;
    temp = temp->next;
    if(temp == todoDel) { break; }
  }

  prev->next = temp->next;
  free(temp->name);
  free(temp);
  return;
}

TodoGroup* createTodoGroup(char *_name) {
  char* newStr = (char*)malloc(strlen(_name) * 2);
  strcpy(newStr, _name);
  TodoGroup *newGroup = (TodoGroup*)malloc(sizeof(TodoGroup));
  newGroup->name = newStr;
  newGroup->todoHead = NULL;
  newGroup->todoTail = NULL;
  newGroup->nextGroup = NULL;
  newGroup->collapsed = false;
  return newGroup;
}

void addGroup(TodoPage** page, char* _name) {
  TodoGroup* newGroup = createTodoGroup(_name);
  if((*page)->groupHead == NULL) {
    (*page)->groupHead = newGroup;
    (*page)->groupTail = newGroup;
    return;
  }

  if((*page)->groupTail != NULL) {
    (*page)->groupTail->nextGroup = newGroup;
    (*page)->groupTail = (*page)->groupTail->nextGroup;
  }
}

void addGroupFromHead(TodoPage** page, char* _name) {
  TodoGroup* newGroup = createTodoGroup(_name);
  if((*page)->groupHead == NULL) {
    (*page)->groupHead = newGroup;
    (*page)->groupTail = newGroup;
  } 
  else {
    newGroup->nextGroup = (*page)->groupHead;
    (*page)->groupHead = newGroup;
  }
}

void deleteTodoGroup(TodoGroup *groupDel, TodoPage **page) {
  TodoPage *_page = *page;
  TodoGroup *tempGroup;
  TodoGroup *prev;
  TodoItem *tempItem;

  if(_page->groupHead != NULL) { tempGroup = _page->groupHead; }
  if(tempGroup == groupDel) {
    if(tempGroup->todoHead != NULL) {
      tempItem = tempGroup->todoHead;
      while(tempGroup->todoHead != NULL) {
        tempItem = tempGroup->todoHead;
        while(tempItem->next != NULL) {
          tempItem = tempItem->next;
        }
        deleteTodo(tempItem, &tempGroup);
      }
    } 

    _page->groupHead = tempGroup->nextGroup;
    free(tempGroup->name);
    free(tempGroup);
    return;
  }

  while(tempGroup->nextGroup != NULL) {
    prev = tempGroup;
    tempGroup = tempGroup->nextGroup;
    if(tempGroup == groupDel) { break; }
  }

  // Delete todos of the group
  if(tempGroup->todoHead != NULL) {
    tempItem = tempGroup->todoHead;
    while(tempGroup->todoHead != NULL) {
      tempItem = tempGroup->todoHead;
      while(tempItem->next != NULL) {
        tempItem = tempItem->next;
      }
      deleteTodo(tempItem, &tempGroup);
    }
  }

  prev->nextGroup = tempGroup->nextGroup;
  free(tempGroup->name);
  free(tempGroup);
  return;
}

TodoPage* createPage(char* _name) {
  TodoPage* newPage = (TodoPage*)malloc(sizeof(TodoPage));
  newPage->name = _name;
  newPage->nextPage = NULL;
  newPage->groupHead = NULL;
  newPage->groupTail = NULL;
  return newPage;
}

void addPage(TodoPage** head, char* _name) {
  TodoPage* newPage = createPage(_name);
  //addGroup(newPage->name, &newPage); 

  if(*head == NULL) {
    *head = newPage;
  } else {
    TodoPage* temp = *head;
    while(temp->nextPage != NULL) {
      temp = temp->nextPage;
    }
    temp->nextPage = newPage;
  }
}

void printStrikethrough(WINDOW *window, int y, int x, char* str) {
  char *c_str = str;
  for(int i = 0; c_str[i] != '\0'; i++) {
    mvwprintw(window, y, x + i, "%c\u0336", c_str[i]); 
  }
}

// *REFACTOR
// BEFORE 131 LINES
void displayPage(WINDOW* mainWindow, PANEL* _panel2, PANEL* _panel3, TodoPage* page, TodoGroup** selectedGroup, TodoItem** selectedItem, int* _highlight, todoMode mode, int* _a, int *sX, int *sY, int xOffset) {
  int printY = 0;
  int a = *_a;
  a = 0;
  int highlight = *_highlight;
  int maxY, maxX;
  getmaxyx(mainWindow, maxY, maxX);

  char collapsedChar = 'V';

  attr_t currentAttribute;

  attr_t a_groupHighlight = A_BOLD | A_STANDOUT;
  attr_t a_groupHighlightUnfocus = A_BOLD | A_UNDERLINE;
  attr_t a_bold = A_BOLD;

  attr_t a_normal = A_NORMAL;
  attr_t a_tickedBox = A_STANDOUT | COLOR_PAIR(2);

  char *tickBox = "[ ]";

  TodoGroup* currentGroup = NULL;
    if(page != NULL){ currentGroup = page->groupHead; }
    while(currentGroup != NULL) {
      init_pair(2, COLOR_GREEN, -1);
      if(printY == highlight) {
        if(mode == TODO) {
          currentAttribute = a_groupHighlight;  

          *selectedGroup = currentGroup;
          *selectedItem = NULL;
          // Calculate move_panel shit 
          move_panel(_panel3, printY + 1, strlen(currentGroup->name) + 5);
        }
        else { 
          currentAttribute = a_groupHighlightUnfocus; 
        }
        *sX = strlen(currentGroup->name) + 20;
        *sY = 1 + printY; 
      }
      else { 
        currentAttribute = a_bold; 
      } 

      wattron(mainWindow, currentAttribute);

      if(currentGroup->collapsed == false) { collapsedChar = 'V'; }
      else { collapsedChar = '>'; }
      mvwprintw(mainWindow, 1 + printY, 1, "%c %s", collapsedChar,currentGroup->name);

      wattroff(mainWindow, currentAttribute);

      printY++;
      a++;
     
      TodoItem *tempItem;
      if(currentGroup != NULL && !currentGroup->collapsed){ 
        tempItem = currentGroup->todoHead; 

        while(tempItem != NULL) {
          if (printY == highlight) { // If selected
            if(mode != TODO && !tempItem->completed) {              
              tickBox = "[ ]";
              currentAttribute = A_UNDERLINE;
            }
            else if(tempItem->completed) {
              tickBox = "[X]";
              currentAttribute = a_tickedBox;
            }
            else {
              tickBox = "[ ]";
              currentAttribute = A_STANDOUT;
            }

            *selectedItem = tempItem;
            *selectedGroup = currentGroup;
            //Calculate move_panel shit
            int bX = strlen(tempItem->name) + 8;
            int bY = printY;
            if(bY > 3) { bY -= 3; } // CEILING
            else if(printY >= maxY - 7) { bY -= (printY - maxY); } // FLOOR
            move_panel(_panel2, bY, bX + xOffset);
            move_panel(_panel3, bY, bX + xOffset);
            *sX = strlen(tempItem->name) + 20;
            *sY = 1 + printY;
          }
          else {
            if(tempItem->completed == true) {              
              tickBox = "[X]";
              currentAttribute = COLOR_PAIR(2);
            }
            else { 
              tickBox = "[ ]";
              currentAttribute = A_NORMAL;
            }
          }
          
          wattron(mainWindow, currentAttribute);
          if(!tempItem->completed) {
            mvwprintw(mainWindow, 1 + printY, 1, "  %s %s", tickBox, tempItem->name);
          } 
          else {
            mvwprintw(mainWindow, 1 + printY, 1, "  %s ", tickBox);
            printStrikethrough(mainWindow, 1 + printY, 7, tempItem->name);
          }
          wattroff(mainWindow, currentAttribute);

          printY++;
          a++;
          tempItem = tempItem->next;
        }
      }
      currentGroup = currentGroup->nextGroup;
    }

  *_a = a;
}

void pageSelect(TodoPage *headPage, TodoPage **activePage) {
  // Display Page List 

}

void initPopup(WINDOW *mainWindow, WINDOW *subWin, PANEL *panel, int h, int w) {
  //panel should be bound to window.
  keypad(mainWindow, FALSE);
  keypad(subWin, TRUE);
  top_panel(panel);
  wresize(subWin, h, w);
  werase(subWin);
  box(subWin, 0 , 0);
}

void endPopup(WINDOW *mainWindow, WINDOW *subWin, PANEL *panel) {
  keypad(subWin, FALSE);
  keypad(mainWindow, TRUE);
  bottom_panel(panel);
}

void initTempPopup(WINDOW *mainWindow, WINDOW **subWin, PANEL **panel, int h, int w, int y, int x) {
  keypad(mainWindow, FALSE);

  *subWin = newwin(h, w, y, x);
  *panel = new_panel(*subWin);

  keypad(*subWin, TRUE);
  top_panel(*panel);
  werase(*subWin);
  box(*subWin, 0 , 0);
  
  update_panels();
  doupdate();
}

void endTempPopup(WINDOW *mainWindow, WINDOW **subWin, PANEL **panel) {
  keypad(*subWin, FALSE);
  keypad(mainWindow, TRUE);
  bottom_panel(*panel);

  del_panel(*panel);
  delwin(*subWin);
  *subWin = NULL;
}

char* textBox(WINDOW *_mainWindow, WINDOW **_subWin, PANEL **_panel, int boxLength, int _y, int _x, char* title) {
  // Single line input text 
  // char *buffer = calloc(boxLength - 2, 1);
  char *buffer = calloc(boxLength - 2, 1);
  initTempPopup(_mainWindow, _subWin, _panel, 3, boxLength, _y, _x);
  curs_set(true);
  
  WINDOW *win = *_subWin;

  bool textBoxOn = true;
  bool e = true;
  int curX = 1;
  int input;
  int length = strlen(buffer);
  
  wattron(win, A_BOLD);
  mvwprintw(win, 0, 1, title);
  wattroff(win, A_BOLD);
  wmove(win, 1, curX);

  while(true) {
    input = wgetch(win);
    length = strlen(buffer);
    mvwprintw(win, 1, 1, buffer);

    if((input == 10 || input == KEY_ENTER) && strlen(buffer) > 0) {
      break;
    }
    else if(input == 27) {
      e = false;
      break;
    }

    if(isprint(input) && curX < boxLength - 2) {
      buffer[curX - 1] = input;
      buffer[curX] = '\0';
      curX++;
      wmove(win, 1, curX); 
    }
    else if((input == KEY_BACKSPACE || input == 127) && curX > 1) {
      curX--;
      buffer[curX - 1] = '\0';
    }
    
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 1, "%-*s", boxLength - 2, buffer);
    wattron(win, A_BOLD);
    mvwprintw(win, 0, 1, title);
    wattroff(win, A_BOLD);
    wmove(win, 1, curX);
    
    update_panels();
    doupdate();
  }
  
  curs_set(false);
  endTempPopup(_mainWindow, _subWin, _panel);
  if(e) { return buffer; }
  else { return "\0"; }
}

void readTodoList(TodoPage** head) {
    if (!head) return;

    FILE *todoDataFile = fopen("/home/user23565/.config/ballsonfire/data.txt", "r");
    if (!todoDataFile) return; // file missing, nothing to load

    fseek(todoDataFile, 0, SEEK_END);
    long length = ftell(todoDataFile);
    fseek(todoDataFile, 0, SEEK_SET);

    if (length <= 0) {
        fclose(todoDataFile);
        return; // empty file
    }

    char *data = malloc(length + 1);
    if (!data) {
        fclose(todoDataFile);
        return; // malloc failed
    }

    fread(data, 1, length, todoDataFile);
    data[length] = '\0';
    fclose(todoDataFile);

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) return;

    int pageCount = cJSON_GetArraySize(root);
    for (int i = 0; i < pageCount; i++) {
        cJSON *pageObject = cJSON_GetArrayItem(root, i);
        if (!pageObject) continue;

        cJSON *pNameNode = cJSON_GetObjectItem(pageObject, "name");
        cJSON *pGroups = cJSON_GetObjectItem(pageObject, "groups");
        if (!pNameNode || !cJSON_IsString(pNameNode) || !pGroups) continue;

        addPage(head, strdup(pNameNode->valuestring));

        // get the last page added
        TodoPage *currentPage = *head;
        while (currentPage->nextPage) currentPage = currentPage->nextPage;

        int groupCount = cJSON_GetArraySize(pGroups);
        for (int j = 0; j < groupCount; j++) {
            cJSON *groupObject = cJSON_GetArrayItem(pGroups, j);
            if (!groupObject) continue;

            cJSON *gNameNode = cJSON_GetObjectItem(groupObject, "name");
            cJSON *gCollapsedNode = cJSON_GetObjectItem(groupObject, "collapsed");
            cJSON *gTodos = cJSON_GetObjectItem(groupObject, "todos");
            if (!gNameNode || !cJSON_IsString(gNameNode) || !gTodos) continue;

            addGroup(&currentPage, strdup(gNameNode->valuestring));

            // get last group added
            TodoGroup *currentGroup = currentPage->groupHead;
            while (currentGroup->nextGroup) currentGroup = currentGroup->nextGroup;

            int todoCount = cJSON_GetArraySize(gTodos);
            for (int k = 0; k < todoCount; k++) {
                cJSON *todoObject = cJSON_GetArrayItem(gTodos, k);
                if (!todoObject) continue;

                cJSON *tNameNode = cJSON_GetObjectItem(todoObject, "name");
                cJSON *tDescriptionNode = cJSON_GetObjectItem(todoObject, "description");
                cJSON *tCompletedNode = cJSON_GetObjectItem(todoObject, "completed");

                if (!tNameNode || !cJSON_IsString(tNameNode)) continue;

                addTodo(&currentGroup,
                        strdup(tNameNode->valuestring),
                        tCompletedNode ? cJSON_IsTrue(tCompletedNode) : false,
                        tDescriptionNode ? strdup(tDescriptionNode->valuestring) : strdup(""));
            }

            // set collapsed state
            currentGroup->collapsed = gCollapsedNode ? cJSON_IsTrue(gCollapsedNode) : false;
        }
    }

    cJSON_Delete(root);
}

void writeTodoList(TodoPage* headPage) { // Write Page
  FILE *todoDataFile = fopen("/home/user23565/.config/ballsonfire/data.txt", "wb");
  if(todoDataFile == NULL) { 
    mkdir("/home/user23565/.config/ballsonfire", 0755);
    writeTodoList(headPage);
    return; 
  }
  
  TodoPage *currentPage; 
  TodoGroup *currentGroup;
  TodoItem *currentItem; 
  
  cJSON *root = cJSON_CreateArray();

  if(headPage != NULL) {
    currentPage = headPage;
    while(currentPage != NULL) {
      
      cJSON *pageObject = cJSON_CreateObject();
      cJSON_AddStringToObject(pageObject, "name",currentPage->name);

      cJSON *groups = cJSON_CreateArray();
      cJSON_AddItemToObject(pageObject, "groups", groups);

      if(currentPage->groupHead != NULL) {
        currentGroup = currentPage->groupHead;
        while(currentGroup != NULL) {
          
          cJSON *groupObject = cJSON_CreateObject();
          cJSON_AddStringToObject(groupObject, "name", currentGroup->name);
          cJSON_AddBoolToObject(groupObject, "collapsed", currentGroup->collapsed);
          
          cJSON *todos = cJSON_CreateArray();
          cJSON_AddItemToObject(groupObject, "todos", todos);

          if(currentGroup->todoHead != NULL) {
            currentItem = currentGroup->todoHead;
            while(currentItem != NULL) {

              cJSON *todoObject = cJSON_CreateObject();
              cJSON_AddStringToObject(todoObject, "name", currentItem->name);
              cJSON_AddStringToObject(todoObject, "description", currentItem->description);
              cJSON_AddBoolToObject(todoObject, "completed", currentItem->completed); 

              cJSON_AddItemToArray(todos, todoObject);
              currentItem = currentItem->next;
            }
          }

          cJSON_AddItemToArray(groups, groupObject);
          currentGroup = currentGroup->nextGroup;
        }
      }

      cJSON_AddItemToArray(root, pageObject);
      currentPage = currentPage->nextPage;
    }
  }

  char* data = cJSON_PrintUnformatted(root);
  fprintf(todoDataFile, data);
  free(data);
  fclose(todoDataFile);

  cJSON_Delete(root);
}

void TodoApp() {
  bool running = true;
  int maxX, maxY;
  getmaxyx(stdscr, maxY, maxX); 
  int aCurX = 2;
  int scrWidth = 100;
  if((maxX/3) > scrWidth) { scrWidth = maxX/3; }
  else if((scrWidth > maxX)) { scrWidth = maxX; }
  int xOffset = (maxX / 2 - scrWidth/2);

  WINDOW *mainWindow = newwin(maxY, scrWidth, 0, xOffset);
  WINDOW *glSubWin; // use for initTempPopup() and endTempPopup() 
  WINDOW *subWin1 = newwin(1, 1, 1, 1);
  WINDOW *subWin2 = newwin(1, 1, 1, 1);
  WINDOW *subWin3 = newwin(1, 1, 1, 1); 

  PANEL *panel0 = new_panel(mainWindow);
  PANEL *glPanel; // use for initTempPopup() and endTempPopup()
  PANEL *panel1 = new_panel(subWin1); // bottom menu
  PANEL *panel2 = new_panel(subWin2); // interact with todo menu 
  PANEL *panel3 = new_panel(subWin3); // are you sure you want to delete group/item?

  todoMode mode = TODO; 

  TodoPage* selectedPage = NULL; 
  TodoPage* headPage = NULL;

  TodoGroup* selectedGroup = NULL;
  TodoItem* selectedItem = NULL;

  int highlight = 0;
  int input = 0;
  int a = 0;
  int sX, sY;
  char addBuffer[256];

  readTodoList(&headPage);

  if(headPage != NULL) {
    selectedPage = headPage;
  }

  keypad(mainWindow, true);
  top_panel(panel0);
  doupdate();

  // MAIN TODO LOOP
  while(running) {
    // GRAPHICS
    werase(mainWindow);
    box(mainWindow, 0, 0);

    if(selectedPage != NULL) {
      char* titleDisplay = selectedPage->name;
      mvwprintw(mainWindow, 0, scrWidth/2 - strlen(titleDisplay)/2, "%s", titleDisplay);
    }
    else {
      // initTempPopup(mainWindow, &glSubWin, &glPanel, 5, 40, (maxY / 2) - 5, (maxX / 2) - 20);
    
      // char *message_str = "There are no pages..."; 
      // mvwprintw(glSubWin, 2, strlen(message_str)/2, message_str);
  
      char *textBoxMessage = "Create Todo Page:";
      int textBoxMessageLength = strlen(textBoxMessage);
      char* pageName = textBox(mainWindow, &glSubWin, &glPanel, 60, maxY/2 + 2, maxX/2 - 30, textBoxMessage);
      if(pageName && (strlen(pageName) > 0 && *pageName != '\0')) {
        addPage(&headPage, pageName);
        selectedPage = headPage;
        continue;
      }

      // endTempPopup(mainWindow, &glSubWin, &glPanel);
    }
    
    // RENDER TODOLIST (page) 
    displayPage(mainWindow, panel2, panel3, selectedPage, &selectedGroup, &selectedItem, &highlight, mode, &a, &sX, &sY, xOffset);
    
    update_panels();
    doupdate(); 

    //INPUT
    if (mode == TODO) { input = wgetch(mainWindow); }
    if(mode == TODO) {
      switch(input) {
        case 'A':
        case 'a':
          mode = ADD;
          break;
        case 'K':
        case 'k':
        case KEY_UP:
          if(highlight <= 0) { highlight = a - 1; } else { highlight--; }
          break;
        case 'J':
        case 'j':
        case KEY_DOWN:
          if(highlight >= a - 1) { highlight = 0; } else { highlight++; }
          break;
        case 27: // ESC
          mode = MENU;
          break;
        case KEY_ENTER:
        case 10: // ENTER
          if(selectedGroup != NULL && selectedItem == NULL) {
            selectedGroup->collapsed = !selectedGroup->collapsed;
          }
          else if(selectedItem != NULL) {
            keypad(mainWindow, FALSE);
            keypad(subWin2, TRUE);
            top_panel(panel2);
            wresize(subWin2, 7, 25);
            werase(subWin2);
            wattron(subWin2, A_BOLD);
            mvwprintw(subWin2, 1, 1, "[1] Mark/Unmark as Done");
            mvwprintw(subWin2, 2, 1, "[2] Rename");
            mvwprintw(subWin2, 3, 1, "[3] Add Comment");
            mvwprintw(subWin2, 4, 1, "[4] Move");
            mvwprintw(subWin2, 5, 1, "[5] Delete");
            wattron(subWin2, A_BOLD);
            box(subWin2, 0, 0);
            
            int select_input = wgetch(subWin2);
            switch(select_input) {
              case '1':
                selectedItem->completed = !selectedItem->completed;
                break;
              case '2':
                break;
              case '3':
                break;
              case '4':
                break;
              case '5':
                deleteTodo(selectedItem, &selectedGroup);
                selectedItem = NULL;
                break;
            }

            bottom_panel(panel2);
            hide_panel(panel2);
            keypad(subWin2, FALSE);
            keypad(mainWindow, TRUE);
          }
          break;
        case 'd': // DELETE GROUPING (not yet individual items)
          if(selectedGroup != NULL) {
            initPopup(mainWindow, subWin3, panel3, 5, 56);
            
            mvwprintw(subWin3, 1, 1, "Are you sure you want to delete this item/group?");
            mvwprintw(subWin3, 2, 1, "(All other elements below this group will be deleted)");
            int a_input = wgetch(subWin3);
            if(a_input == KEY_ENTER || a_input == 10) {
              if(selectedItem == NULL ) {
                deleteTodoGroup(selectedGroup, &selectedPage);
                selectedGroup = NULL;
              } 
              else {
                deleteTodo(selectedItem, &selectedGroup);
              } 
            }

            endPopup(mainWindow, subWin3, panel3);
            hide_panel(panel3);
          }
          break;
        case 's':
        case 'S':
          writeTodoList(selectedPage);
          initTempPopup(mainWindow, &glSubWin, &glPanel, 3, 40, 1, maxX/2 + scrWidth/2 - 41);
          mvwprintw(glSubWin, 1, 1, "✔ Data Saved...");
          wgetch(glSubWin);
          endTempPopup(mainWindow, &glSubWin, &glPanel);
          break;
        case 'm':
        case 'M':
          if(selectedItem != NULL) { selectedItem->completed = !selectedItem->completed; }
          break;
        case 'r':
        case 'R':
          int x = (scrWidth - 2 - sX);
          if(selectedItem != NULL) {
            int len = strlen(selectedItem->name);
            char* todoName = (char*)malloc(len + 1);
            strcpy(todoName, selectedItem->name);
            todoName[len] = '\0';
            char renameStr[128];
            snprintf(renameStr, sizeof(renameStr), "RENAME: \"%s\"", todoName);
            char* newStr = textBox(mainWindow, &glSubWin, &glPanel, x, sY - 1, sX + xOffset, renameStr);
            if(*newStr != '\0') { selectedItem->name = newStr; }
            free(todoName);
            break;
          }
          else {
            int len = strlen(selectedGroup->name);
            char* groupName = (char*)malloc(len + 1);
            strcpy(groupName, selectedGroup->name);
            groupName[len] = '\0';
            char renameStr[128];
            snprintf(renameStr, sizeof(renameStr), "RENAME: \"%s\"", groupName);
            char* newStr = textBox(mainWindow, &glSubWin, &glPanel, x, sY - 1, sX + xOffset, renameStr);
            if(*newStr != '\0') { selectedGroup->name = newStr; }
            free(groupName);
            break;
          }
      }
    }
    else if(mode == ADD) {
      aCurX = 0;
      keypad(mainWindow, FALSE);
      keypad(subWin1, TRUE);
      move_panel(panel1, maxY - 5, xOffset + 1);
      top_panel(panel1);
      werase(subWin1);
      wresize(subWin1, 4, 20);
      mvwprintw(subWin1, 1, 1, "[i] Add Item");
      mvwprintw(subWin1, 2, 1, "[g] Add Group");
      box(subWin1, 0, 0);

      bool a = false;
      bool b = false;
      char adding[32];

      
      for(int i = 0; i < (int)(sizeof(addBuffer)); i++) {
        addBuffer[i] = '\0';
      }

      while(input = wgetch(subWin1)) {
        if(!a) {
          if(input == 'i' || input == 'I' || input == '1') {
            strcpy(adding, "Item");
            werase(subWin1);
            a = true;
            input = 0;
            goto addingit;
          } 
          else if(input == 'g' || input == 'G' || input == '2') {
            strcpy(adding, "Group");
            werase(subWin1);
            a = true;   
            input = 0;
            goto addingit;
          }
          else if(input == 27) {
            werase(subWin1);
            curs_set(false);
            keypad(subWin1, FALSE);
            mode = TODO;
            goto exitAdd;
          }
        } 
        else {
        addingit:
          hide_panel(panel1);
          update_panels();
          doupdate();
          curs_set(1);
          if(!b) {
            werase(subWin1);
            wmove(subWin1, 1, 2);
            mvwprintw(subWin1, 0, 2, "Add %s", adding);
            mvwprintw(subWin1, 1, 1, ":");
            move_panel(panel1, maxY - 4, xOffset + 1);
            wresize(subWin1, 3, scrWidth - 2); 
          }
          werase(subWin1);
          box(subWin1, 0, 0);
          mvwprintw(subWin1, 0, 2, "Add %s", adding);
          mvwprintw(subWin1, 1, 1, ":");

          if(isprint(input) && aCurX < 127) {
            wmove(subWin1, 1, aCurX);
            addBuffer[aCurX] = input;
            aCurX++;
          } 
          else if((input == 263 || input == KEY_BACKSPACE) && aCurX > 0) {
            aCurX--;
            wmove(subWin1, 1, aCurX);
            addBuffer[aCurX] = '\0';
            mvwhline(subWin1, 1, 2, ' ', maxX - 4);
            // mvwprintw(mainWindow, 10, 23, "haha tite");
          } 
          else if(input == 27) {
            werase(subWin1);
            curs_set(false);
            keypad(subWin1, FALSE);
            mode = TODO;
            goto exitAdd;
          } 
          else if((input == 10 || input == '\n' || input == 343 || input == KEY_ENTER)) {
            // mvwprintw(mainWindow, 10, 23, "haha tite");
            if(strcmp(adding, "Item") == 0) {
              if(selectedGroup == NULL) {
                addGroupFromHead(&selectedPage, "NEW GROUP");
                addTodoFromHead(&selectedPage->groupHead, addBuffer, false, NULL);
                selectedGroup = selectedPage->groupHead;
              }
              else {
                addTodoFromHead(&selectedGroup, addBuffer, false, NULL);
              }
              bottom_panel(panel1);
              mode = TODO;
              memset(addBuffer, 0, sizeof(addBuffer));
              goto exitAdd;
            } 
            else if(strcmp(adding, "Group") == 0) {
              addGroupFromHead(&selectedPage, addBuffer);
              bottom_panel(panel1);
              mode = TODO;
              memset(addBuffer, 0, sizeof(addBuffer));
              goto exitAdd;
            }
          }
          mvwprintw(subWin1, 1, 2, "%s", addBuffer);
        }
      }
      exitAdd:
      werase(subWin1);
      bottom_panel(panel1);
      hide_panel(panel1);
      curs_set(false);
      keypad(subWin1, FALSE);
      keypad(mainWindow, TRUE);
      mode = TODO;
    } 
    else if(mode == MENU) {
      running = false; // Placeholder :)
      if(input == 27) {
        running = false;
      }
    } 
  }
} 

void highlightWindow(WINDOW* win) { // (FIX??) COLOR PAIR CAN ONLY BE USED ONCE????
  init_pair(1, COLOR_RED, -1);

  wattron(win, COLOR_PAIR(1));
  box(win, 0, 0);
  wattroff(win, COLOR_PAIR(1));
}

void MainMenu() {
  bool running = true;
  int w = 50;
  int h = 20;
  int maxX, maxY;
  char *title = "C TERMINAL PRODUCTIVITY APP";
  int titleLength = (int)strlen(title);

  ApplicationItem todoApp = {"Todo List (*WORK IN PROGRESS)", TodoApp};
  ApplicationItem noteTakingApp = {"Notes (NOT IMPLEMENTED)", placeholderVoid};
  ApplicationItem quizApp = {"Quiz (NOT IMPLEMENTED)", placeholderVoid};
  ApplicationItem flashCards = {"Flashcards (NOT IMPLEMENTED)", placeholderVoid};
  ApplicationItem expenseTracker = {"Expense Tracker (NOT IMPLEMENTED)", placeholderVoid};
  ApplicationItem appList[] = {todoApp, noteTakingApp, quizApp, flashCards, expenseTracker};
  int appListLength = (int)(sizeof(appList) / sizeof(appList[0]));

  getmaxyx(stdscr, maxY, maxX);

  WINDOW* window = newwin(h, w, (maxY - h) / 2, (maxX - w) / 2);
  int highlight = 0;
  int choice = 0;

  keypad(window, TRUE);
  while(running) {
    werase(window);
    //highlightWindow(window);
    box(window, 0, 0);
    mvwprintw(window, 0, (w - titleLength) / 2, "%s", title);
    for(int i = 0; i < appListLength; i++) {
      if(i == highlight) {
        wattron(window, A_STANDOUT);
        mvwprintw(window, i + 2, 1, "%s", appList[i].title);
        wattroff(window, A_STANDOUT);
      }
      else {
        mvwprintw(window, i + 2, 1, "%s", appList[i].title);
      }
    }
    
    int input = wgetch(window);
    switch(input) {
      case KEY_UP:
        if(highlight <= 0) { highlight = appListLength - 1; } else { highlight--;}
        break;
      case KEY_DOWN:
        if(highlight >= appListLength - 1) { highlight = 0; } else { highlight++; }
        break;
      case 10:
      case KEY_ENTER:
        choice = highlight;
        running = false;
        werase(window);
        wrefresh(window);
        appList[choice].run();
        break;
    }
    
    wrefresh(window);
  }
  wrefresh(window);
  delwin(window);
  refresh();
  appList[choice].run();
  keypad(window, FALSE);
}

int main() {
  setlocale(LC_ALL, "");
  initscr();
  //raw();
  start_color();
  use_default_colors();
  noecho();
  cbreak();
  curs_set(false);
  ESCDELAY = 25;

  MainMenu();

  endwin();
  return 0;
}

