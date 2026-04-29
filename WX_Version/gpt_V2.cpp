#include <wx/wx.h>
#include <wx/dir.h>
#include <fstream>
#include <string>

class MyFrame : public wxFrame {
public:
    MyFrame()
        : wxFrame(nullptr, wxID_ANY, "TXT Search Tool",
                  wxDefaultPosition, wxSize(750, 550)) {

        wxPanel* panel = new wxPanel(this);

        wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

        folderLabel = new wxStaticText(panel, wxID_ANY, "No folder selected");

        wxButton* folderBtn = new wxButton(panel, wxID_ANY, "Select Folder");
        input = new wxTextCtrl(panel, wxID_ANY);
        wxButton* searchBtn = new wxButton(panel, wxID_ANY, "Search");

        output = new wxTextCtrl(panel, wxID_ANY, "",
                                wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY);

        vbox->Add(folderLabel, 0, wxEXPAND | wxALL, 5);
        vbox->Add(folderBtn, 0, wxEXPAND | wxALL, 5);
        vbox->Add(input, 0, wxEXPAND | wxALL, 5);
        vbox->Add(searchBtn, 0, wxEXPAND | wxALL, 5);
        vbox->Add(output, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(vbox);

        folderBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSelectFolder, this);
        searchBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSearch, this);
    }

private:
    wxTextCtrl* input;
    wxTextCtrl* output;
    wxStaticText* folderLabel;

    wxString selectedFolder = ".";

    void OnSelectFolder(wxCommandEvent&) {
        wxDirDialog dlg(this, "Choose a folder");

        if (dlg.ShowModal() == wxID_OK) {
            selectedFolder = dlg.GetPath();
            folderLabel->SetLabel("Folder: " + selectedFolder);
        }
    }

    void OnSearch(wxCommandEvent&) {
        wxString target = input->GetValue();
        output->Clear();

        if (target.IsEmpty()) {
            output->AppendText("Please enter a search word.\n");
            return;
        }

        wxDir dir(selectedFolder);
        if (!dir.IsOpened()) {
            output->AppendText("Could not open selected folder.\n");
            return;
        }

        wxString filename;
        bool cont = dir.GetFirst(&filename, "*.txt", wxDIR_FILES);

        bool found = false;

        while (cont) {
            wxString fullPath = selectedFolder + "/" + filename;

            std::ifstream file(fullPath.ToStdString());
            std::string line;
            int lineNumber = 0;

            while (std::getline(file, line)) {
                lineNumber++;

                if (line.find(target.ToStdString()) != std::string::npos) {
                    output->AppendText(
                        filename + " [line " +
                        std::to_string(lineNumber) + "]: " +
                        line + "\n"
                    );
                    found = true;
                }
            }

            cont = dir.GetNext(&filename);
        }

        if (!found) {
            output->AppendText("No matches found.\n");
        }
    }
};

class MyApp : public wxApp {
public:
    bool OnInit() override {
        MyFrame* frame = new MyFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);