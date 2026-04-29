#include <wx/wx.h>

class App : public wxApp {
public:
    bool OnInit() {
        wxMessageBox("It works!");
        return false;
    }
};

wxIMPLEMENT_APP(App);
