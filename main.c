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

// LAST COMMIT: 12-04-2025
// 11-22-2025
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
    struct TodoItem *prev;
    struct TodoItem *next;
} TodoItem;

typedef struct TodoGroup {
    char *name;
    TodoItem *todoHead;
    TodoItem *todoTail;
    struct TodoGroup *prevGroup;
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

typedef struct TDLContext {
    WINDOW *mainWindow;
    WINDOW *subWin1;
    WINDOW *subWin2;
    WINDOW *subWin3;
    WINDOW *popupWindow;

    PANEL *mainPanel;
    PANEL *panel1;
    PANEL *panel2;
    PANEL *panel3; 
    PANEL *popupPanel;

    int maxX;
    int maxY;
    int scrWidth;
    int xOffset;

    TodoPage *headPage;
    TodoPage *selectedPage;
    TodoGroup *selectedGroup;
    TodoItem *selectedItem;

    todoMode mode;
    int highlight;
    int cursorX;
    int cursorY;
    int itemCount;
    int input;

    char addBuffer[256];
    char textBoxBuffer[256];

    char dataPath[256];
    
    bool running;
    bool popupActive;
} TDLContext;

TodoItem *createTodo(char *_title, bool _completed, char* _description) {
    char* newStr = (char*)malloc(strlen(_title) * 2);
    strcpy(newStr, _title);
    TodoItem* newTodo = (TodoItem*)malloc(sizeof(TodoItem));
    newTodo->completed = _completed;
    newTodo->name = newStr;
    newTodo->description = _description;
    newTodo->prev = NULL;
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
        newItem->prev = _group->todoTail;
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
    newGroup->prevGroup = NULL;
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
        newGroup->prevGroup = (*page)->groupTail;
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
void displayPage(TDLContext *ctx,int* _a, int *sX, int *sY) {

    
    // WINDOW* mainWindow, PANEL* _panel2, PANEL* _panel3, TodoPage* page, 
    // TodoGroup** selectedGroup, TodoItem** selectedItem, int* _highlight, 
    // todoMode mode, int* _a, int *sX, int *sY, int xOffset

    WINDOW *mainWindow = ctx->mainWindow;
    PANEL *_panel2 = ctx->panel2;
    PANEL *_panel3 = ctx->panel3;
    TodoPage *page = ctx->selectedPage;
    TodoGroup **selectedGroup = &ctx->selectedGroup;
    TodoItem **selectedItem = &ctx->selectedItem;
    todoMode mode = ctx->mode;
    int xOffset = ctx->xOffset;


    int printY = 0;
    int a = *_a;
    a = 0;
    int highlight = ctx->highlight;
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


// CREATE OWN WINDOW TEXTBOX
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

/*

void moveMode() {
    // Move item position logic
    bool moveMode = true; // true is item false is for groups 
    bool canMove = true;

    TodoItem *currentItemPosition = NULL;
    TodoGroup *currentGroupPosition = NULL;

    TodoItem *movingItem = NULL;
    TodoGroup *movingGroup = NULL;

    if(!selectedGroup && selectedItem) { moveMode = true; }
    else if(selectedGroup && selectedItem) { moveMode = false; }

    if(moveMode) { // Move item
        currentItemPosition = selectedItem;
        movingItem = selectedItem;

        while(canMove) {

            int moveInput = wgetch(mainWindow);
            if(moveInput == KEY_UP) {
                if(movingItem->prev != NULL) {
                    movingItem->prev->next = movingItem->next;
                    movingItem->next->prev = movingItem->prev;
                    movingItem->prev->prev = movingItem;
                    movingItem->next = movingItem->prev;
                    movingItem->prev = movingItem->prev->prev;
                    movingItem->prev->next = movingItem;
                }
            }
            else if(moveInput == KEY_ENTER) {
                break;
            }
        } // end loop
    }
    else { // Move group

    }

}

*/

void addMode(TDLContext *ctx) {
    WINDOW *mainWindow = ctx->mainWindow;
    WINDOW *subWin1 = ctx->subWin1;

    PANEL *panel1 = ctx->panel1;
    int maxX = ctx->maxX;
    int maxY = ctx->maxY;
    int xOffset = ctx->xOffset;
    int scrWidth = ctx->scrWidth;

    char *addBuffer = ctx->addBuffer;

    TodoGroup *selectedGroup = ctx->selectedGroup;
    TodoPage *selectedPage = ctx->selectedPage;

    int aCurX = 2;
    int bufIndex = 0;
    int input = 0;

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

    memset(addBuffer, 0, 128);

    while(1) {
        input = wgetch(subWin1);

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
                ctx->mode = TODO;
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
                addBuffer[bufIndex++] = input;
                addBuffer[bufIndex] = '\0';
                aCurX++;
            } 
            else if((input == 263 || input == KEY_BACKSPACE) && aCurX > 0) {
                aCurX--;
                bufIndex--;
                wmove(subWin1, 1, aCurX);
                addBuffer[bufIndex] = '\0';
                mvwhline(subWin1, 1, 2, ' ', maxX - 4);
            } 
            else if(input == 27) {
                werase(subWin1);
                curs_set(false);
                keypad(subWin1, FALSE);
                ctx->mode = TODO;
                goto exitAdd;
            } 
            else if((input == 10 || input == '\n' || input == 343 || input == KEY_ENTER)) {
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
                    ctx->mode = TODO;
                    memset(addBuffer, 0, sizeof(addBuffer));
                    goto exitAdd;
                } 
                else if(strcmp(adding, "Group") == 0) {
                    addGroupFromHead(&selectedPage, addBuffer);
                    bottom_panel(panel1);
                    ctx->mode = TODO;
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
        ctx->mode = TODO; 

}

void TodoApp() {
    TDLContext ctx = {0}; // GLOBAL VARIBAL

    ctx.running = true;
    getmaxyx(stdscr, ctx.maxY, ctx.maxX); 
    ctx.scrWidth = 100;
    if((ctx.maxX/3) > ctx.scrWidth) { ctx.scrWidth = ctx.maxX/3; }
    else if((ctx.scrWidth > ctx.maxX)) { ctx.scrWidth = ctx.maxX; }
    ctx.xOffset = (ctx.maxX / 2 - ctx.scrWidth/2);

    ctx.mainWindow = newwin(ctx.maxY, ctx.scrWidth, 0, ctx.xOffset);
    ctx.subWin1 = newwin(1, 1, 1, 1);
    ctx.subWin2 = newwin(1, 1, 1, 1);
    ctx.subWin3 = newwin(1, 1, 1, 1); 

    ctx.mainPanel = new_panel(ctx.mainWindow);
    ctx.panel1 = new_panel(ctx.subWin1); // bottom menu FOR ADDING SHIT
    ctx.panel2 = new_panel(ctx.subWin2); // interact with todo menu 
    ctx.panel3 = new_panel(ctx.subWin3); // are you sure you want to delete group/item?

    ctx.mode = TODO; 

    ctx.highlight = 0;
    int input = 0;
    int a = 0;
    int sX, sY;

    readTodoList(&ctx.headPage);

    if(ctx.headPage != NULL) {
        ctx.selectedPage = ctx.headPage;
    }

    keypad(ctx.mainWindow, true);
    top_panel(ctx.mainPanel);
    doupdate();

    // MAIN TODO LOOP
    while(ctx.running) {
        // GRAPHICS
        werase(ctx.mainWindow);
        box(ctx.mainWindow, 0, 0);

        if(ctx.selectedPage != NULL) {
            char* titleDisplay = ctx.selectedPage->name;
            mvwprintw(ctx.mainWindow, 0, ctx.scrWidth/2 - strlen(titleDisplay)/2, "%s", titleDisplay);
        }
        else {
            // initTempPopup(mainWindow, &glSubWin, &glPanel, 5, 40, (maxY / 2) - 5, (maxX / 2) - 20);

            // char *message_str = "There are no pages..."; 
            // mvwprintw(glSubWin, 2, strlen(message_str)/2, message_str);

            char *textBoxMessage = "Create Todo Page:";
            int textBoxMessageLength = strlen(textBoxMessage);
            char* pageName = textBox(ctx.mainWindow, &ctx.popupWindow, &ctx.popupPanel, 60, ctx.maxY/2 + 2, ctx.maxX/2 - 30, textBoxMessage);
            if(pageName && (strlen(pageName) > 0 && *pageName != '\0')) {
                addPage(&ctx.headPage, pageName);
                ctx.selectedPage = ctx.headPage;
                continue;
            }

            // endTempPopup(mainWindow, &glSubWin, &glPanel);
        }

        // RENDER TODOLIST (page)
        // displayPage(ctx.mainWindow, ctx.panel2, ctx.panel3, ctx.selectedPage, &ctx.selectedGroup, &ctx.selectedItem, &ctx.highlight, ctx.mode, &a, &sX, &sY, ctx.xOffset);
        displayPage(&ctx, &a, &sX, &sY);

        update_panels();
        doupdate(); 

        //INPUT
        if (ctx.mode == TODO) { input = wgetch(ctx.mainWindow); }
        if(ctx.mode == TODO) {
            switch(input) {
                case 'A':
                case 'a':
                    ctx.mode = ADD;
                    addMode(&ctx);
                    break;
                case 'K':
                case 'k':
                case KEY_UP:
                    if(ctx.highlight <= 0) { ctx.highlight = a - 1; } else { ctx.highlight--; }
                    break;
                case 'J':
                case 'j':
                case KEY_DOWN:
                    if(ctx.highlight >= a - 1) { ctx.highlight = 0; } else { ctx.highlight++; }
                    break;
                case 27: // ESC
                    ctx.mode = MENU;
                    break;
                case 'M':
                case 'm':
                    //moveMode();
                    break;
                case KEY_ENTER:
                case 10: // ENTER
                    if(ctx.selectedGroup != NULL && ctx.selectedItem == NULL) {
                        (ctx.selectedGroup)->collapsed = !(ctx.selectedGroup)->collapsed;
                    }
                    else if(ctx.selectedItem != NULL) {
                        keypad(ctx.mainWindow, FALSE);
                        keypad(ctx.subWin2, TRUE);
                        top_panel(ctx.panel2);
                        wresize(ctx.subWin2, 7, 25);
                        werase(ctx.subWin2);
                        wattron(ctx.subWin2, A_BOLD);
                        mvwprintw(ctx.subWin2, 1, 1, "[1] Mark/Unmark as Done");
                        mvwprintw(ctx.subWin2, 2, 1, "[2] Rename");
                        mvwprintw(ctx.subWin2, 3, 1, "[3] Add Comment");
                        mvwprintw(ctx.subWin2, 4, 1, "[4] Move");
                        mvwprintw(ctx.subWin2, 5, 1, "[5] Delete");
                        wattron(ctx.subWin2, A_BOLD);
                        box(ctx.subWin2, 0, 0);

                        int select_input = wgetch(ctx.subWin2);
                        switch(select_input) {
                        case '1':
                            (ctx.selectedItem)->completed = !(ctx.selectedItem)->completed;
                            break;
                        case '2':
                            goto rename;
                            break;
                        case '3':
                            break;
                        case '4':
                            //moveMode();
                        }
                    } 
                    break;
                case '5':
                    deleteTodo(ctx.selectedItem, &ctx.selectedGroup);
                    ctx.selectedItem = NULL;
                    break;
                    bottom_panel(ctx.panel2);
                    hide_panel(ctx.panel2);
                    keypad(ctx.subWin2, FALSE);
                    keypad(ctx.mainWindow, TRUE);
                    break;
                case 'd': 
                    if(ctx.selectedGroup != NULL) {
                        initPopup(ctx.mainWindow, ctx.subWin3, ctx.panel3, 5, 56);

                        mvwprintw(ctx.subWin3, 1, 1, "Are you sure you want to delete this item/group?");
                        mvwprintw(ctx.subWin3, 2, 1, "(All other elements below this group will be deleted)");
                        int a_input = wgetch(ctx.subWin3);
                        if(a_input == KEY_ENTER || a_input == 10) {
                            if(ctx.selectedItem == NULL ) {
                                deleteTodoGroup(ctx.selectedGroup, &ctx.selectedPage);
                                ctx.selectedGroup = NULL;
                            } 
                            else {
                                deleteTodo(ctx.selectedItem, &ctx.selectedGroup);
                            } 
                        }

                        endPopup(ctx.mainWindow, ctx.subWin3, ctx.panel3);
                        hide_panel(ctx.panel3);
                    }
                    break;
                case 's':
                case 'S':
                    writeTodoList(ctx.selectedPage);
                    // AUTOMATE (add timer?)
                    initTempPopup(ctx.mainWindow, &ctx.popupWindow, &ctx.popupPanel, 3, 40, 1, ctx.maxX/2 + ctx.scrWidth/2 - 41);
                    mvwprintw(ctx.popupWindow, 1, 1, "✔ Data Saved...");
                    wgetch(ctx.popupWindow);
                    endTempPopup(ctx.mainWindow, &ctx.popupWindow, &ctx.popupPanel);
                    break;
                case ' ':
                    if(ctx.selectedItem != NULL) { ctx.selectedItem->completed = !ctx.selectedItem->completed; }
                    break;
                case 'r':
                case 'R':
                rename: // REFACTOR
                    int x = (ctx.scrWidth - 2 - sX);
                    if(ctx.selectedItem != NULL) {
                        int len = strlen(ctx.selectedItem->name);
                        char* todoName = (char*)malloc(len + 1);
                        strcpy(todoName, ctx.selectedItem->name);
                        todoName[len] = '\0';
                        char renameStr[128];
                        snprintf(renameStr, sizeof(renameStr), "RENAME: \"%s\"", todoName);
                        char* newStr = textBox(ctx.mainWindow, &ctx.popupWindow, &ctx.popupPanel, x, sY - 1, sX + ctx.xOffset, renameStr);
                        if(*newStr != '\0') { ctx.selectedItem->name = newStr; }
                        free(todoName);
                    }
                    else {
                        int len = strlen(ctx.selectedGroup->name);
                        char* groupName = (char*)malloc(len + 1);
                        strcpy(groupName, ctx.selectedGroup->name);
                        groupName[len] = '\0';
                        char renameStr[128];
                        snprintf(renameStr, sizeof(renameStr), "RENAME: \"%s\"", groupName);
                        char* newStr = textBox(ctx.mainWindow, &ctx.popupWindow, &ctx.popupPanel, x, sY - 1, sX + ctx.xOffset, renameStr);
                        if(*newStr != '\0') { ctx.selectedGroup->name = newStr; }
                        free(groupName);
                    }
                    break;
            }
        }  
        else if(ctx.mode == MENU) {
            ctx.running = false; // Placeholder :)
            if(input == 27) {
                ctx.running = false;
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

