#ifndef ADMIN_VIEW_
#define ADMIN_VIEW_

#include <application.h>

class adminView{
    private:
        static void ZoomEventCallback(nlohmann::json e);
        static void ViewMatrixCallback(PrimitiveContainer *p);
        static void RenderUI(void *p);
    public:
        EasyRender::EasyRenderGui *ui;
        adminView(){
            
        };
        void PreInit();
        void Init();
        void Tick();
        void MakeActive();
        void Close();
};

#endif