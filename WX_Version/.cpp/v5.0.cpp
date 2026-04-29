#include <wx/wx.h>
#include <fstream>
#include <sstream>

bool containsWord(const std::string& line, const std::string& word) {
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token == word) return true;
    }
    return false;
}

class MyFrame : public wxFrame {
private:
    wxTextCtrl* filePathCtrl;
    wxTextCtrl* wordCtrl;
    wxTextCtrl* outputCtrl;

    std::string filePath;

public:
    MyFrame() : wxFrame(nullptr, wxID_ANY, "File Search Tool", wxDefaultPosition, wxSize(700, 500)) {

        wxPanel* panel = new wxPanel(this);

        wxButton* fileBtn = new wxButton(panel, wxID_ANY, "Select File");
        filePathCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(400, -1));

        wxStaticText* wordLabel = new wxStaticText(panel, wxID_ANY, "Word:");
        wordCtrl = new wxTextCtrl(panel, wxID_ANY);

        wxButton* runBtn = new wxButton(panel, wxID_ANY, "Enter");

        outputCtrl = new wxTextCtrl(panel, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        // Layout
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* fileSizer = new wxBoxSizer(wxHORIZONTAL);
        fileSizer->Add(fileBtn);
        fileSizer->Add(filePathCtrl, 1, wxLEFT, 10);

        wxBoxSizer* wordSizer = new wxBoxSizer(wxHORIZONTAL);
        wordSizer->Add(wordLabel);
        wordSizer->Add(wordCtrl, 1, wxLEFT, 10);
        wordSizer->Add(runBtn, 0, wxLEFT, 10);

        sizer->Add(fileSizer, 0, wxEXPAND | wxALL, 10);
        sizer->Add(wordSizer, 0, wxEXPAND | wxALL, 10);
        sizer->Add(outputCtrl, 1, wxEXPAND | wxALL, 10);

        panel->SetSizer(sizer);

        // Events
        fileBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSelectFile, this);
        runBtn->Bind(wxEVT_BUTTON, &MyFrame::OnRunSearch, this);
    }

    void OnSelectFile(wxCommandEvent&) {
        wxFileDialog openFileDialog(
            this, "Open file", "", "",
            "Text files (*.txt)|*.txt|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST
        );

        if (openFileDialog.ShowModal() == wxID_OK) {
            filePath = openFileDialog.GetPath().ToStdString();
            filePathCtrl->SetValue(filePath);
        }
    }

    void OnRunSearch(wxCommandEvent&) {
        outputCtrl->Clear();

        std::string word = wordCtrl->GetValue().ToStdString();

        std::ifstream inFile(filePath);
        if (!inFile) {
            outputCtrl->AppendText("File not found!\n");
            return;
        }

        std::string line;
        int lineNumber = 0;
        int hits = 0;

        while (std::getline(inFile, line)) {
            ++lineNumber;

            if (containsWord(line, word)) {
                if (line.find("!") == std::string::npos) {
                    outputCtrl->AppendText(
                        std::to_string(lineNumber) + ". " + line + "\n"
                    );
                    hits++;
                }
            }
        }

        outputCtrl->AppendText("\nTotal hits: " + std::to_string(hits) + "\n");
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