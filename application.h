#ifndef APPLICATION_H
#define APPLICATION_H

void Application_Initialize(HWND hWnd);
void Application_Finalize();
void Application_Update(float delta_time);
void Application_FixedUpdate();
void Application_Draw();

#endif // APPLICATION_H
