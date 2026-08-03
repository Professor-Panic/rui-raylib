#include "raylib.h"
#include "rui.h"
int main(){
    InitWindow(1200,800,"Test");
    rui::Context ui;
    if(!ui.LoadMarkupFile("ui/ui.xml"))return 1;
    if(!ui.LoadStyleFile("ui/ui.css")){
        TraceLog(LOG_INFO,"CANT load css"); return 1;
    };
    while (!WindowShouldClose())
    {
        ui.Update();
        BeginDrawing();
        ClearBackground(BLANK);
        ui.Draw();
        EndDrawing();
    }
    CloseWindow();
}