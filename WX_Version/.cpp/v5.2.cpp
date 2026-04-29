#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/dnd.h>
#include <fstream>
#include <sstream>
#include <vector>

// ---------- Logic ----------
bool containsWord(const std::string& line, const std::string& word) {
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token == word) return true;
    }
    return false;
}

// Forward declaration
class MyFrame;

// ---------- Drag & Drop ----------
class FileDropTarget : public wxFileDropTarget {
private:
    MyFrame* frame;

public:
    FileDropTarget(MyFrame* f) : frame(f) {}

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override;
};

// ---------- Main Frame ----------
class MyFrame : public wxFrame {
private:
    wxTextCtrl* filePathCtrl;
    wxTextCtrl* wordCtrl;
    wxTextCtrl* outputCtrl;
    wxChoice* displayChoice;
    wxButton* exportBtn;

    std::string filePath;

    struct Row {
        int lineNumber;
        std::string w1;
        std::string w2;
    };

    std::vector<Row> results;

public:
    MyFrame() : wxFrame(nullptr, wxID_ANY, "Search Tool", wxDefaultPosition, wxSize(800, 600)) {

        wxPanel* panel = new wxPanel(this);

        wxButton* fileBtn = new wxButton(panel, wxID_ANY, "Select File");
        filePathCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(500, -1));

        wxStaticText* wordLabel = new wxStaticText(panel, wxID_ANY, "Word:");
        wordCtrl = new wxTextCtrl(panel, wxID_ANY);

        wxString choices[] = { "Full line", "First 2 words" };
        displayChoice = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 2, choices);
        displayChoice->SetSelection(0);

        exportBtn = new wxButton(panel, wxID_ANY, "Export CSV");
        exportBtn->Disable();

        outputCtrl = new wxTextCtrl(panel, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        // Layout
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* fileSizer = new wxBoxSizer(wxHORIZONTAL);
        fileSizer->Add(fileBtn, 0, wxRIGHT, 10);
        fileSizer->Add(filePathCtrl, 1);

        wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
        inputSizer->Add(wordLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        inputSizer->Add(wordCtrl, 1, wxRIGHT, 10);
        inputSizer->Add(displayChoice, 0, wxRIGHT, 10);
        inputSizer->Add(exportBtn, 0);

        mainSizer->Add(fileSizer, 0, wxEXPAND | wxALL, 10);
        mainSizer->Add(inputSizer, 0, wxEXPAND | wxALL, 10);
        mainSizer->Add(outputCtrl, 1, wxEXPAND | wxALL, 10);

        panel->SetSizer(mainSizer);

        // Events
        fileBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSelectFile, this);
        wordCtrl->Bind(wxEVT_TEXT, &MyFrame::OnLiveSearch, this);
        displayChoice->Bind(wxEVT_CHOICE, &MyFrame::OnLiveSearch, this);
        exportBtn->Bind(wxEVT_BUTTON, &MyFrame::OnExport, this);

        // Drag & Drop
        this->SetDropTarget(new FileDropTarget(this));
    }

    // ---------- File ----------
    void SetFile(const std::string& path) {
        filePath = path;
        filePathCtrl->SetValue(path);
        RunSearch();
    }

    void OnSelectFile(wxCommandEvent&) {
        wxFileDialog openFileDialog(
            this, "Open file", "", "",
            "Modules (*.mod)|*.mod|Text Files (*.txt)|*.txt|All Files (*.*)|*.*",	
			
            wxFD_OPEN | wxFD_FILE_MUST_EXIST
        );

        if (openFileDialog.ShowModal() == wxID_OK) {
            SetFile(openFileDialog.GetPath().ToStdString());
        }
    }

    // ---------- Live Search ----------
    void OnLiveSearch(wxCommandEvent&) {
        RunSearch();
    }

    void RunSearch() {
        outputCtrl->Clear();
        results.clear();
        exportBtn->Disable();

        std::string word = wordCtrl->GetValue().ToStdString();
        if (filePath.empty() || word.empty()) return;

        std::ifstream inFile(filePath);
        if (!inFile) {
            outputCtrl->AppendText("File not found!\n");
            return;
        }

        int choice = displayChoice->GetSelection();

        std::string line;
        int lineNumber = 0;
        int hits = 0;

        while (std::getline(inFile, line)) {
            ++lineNumber;

            if (!containsWord(line, word)) continue;

            // Remove comments
            size_t pos = line.find("!");
            if (pos != std::string::npos) {
                line = line.substr(0, pos);
            }

            std::string w1, w2;

            if (choice == 0) {
                w1 = line;
                w2 = "";
            } else {
                for (char &c : line) {
                    if (c == ',') c = ' ';
                }

                std::istringstream iss(line);
                iss >> w1 >> w2;
            }

            results.push_back({ lineNumber, w1, w2 });

            // Display nicely (still readable)
            std::string display = std::to_string(lineNumber) + " | " + w1;
            if (!w2.empty()) display += " " + w2;

            outputCtrl->AppendText(display + "\n");
            hits++;
        }

        outputCtrl->AppendText("\nTotal: " + std::to_string(hits) + "\n");

        if (!results.empty()) {
            exportBtn->Enable();
        }
    }

    // ---------- Export ----------
    void OnExport(wxCommandEvent&) {
        wxFileDialog saveFileDialog(
            this, "Save CSV", "", "",
            "CSV files (*.csv)|*.csv",
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT
        );

        if (saveFileDialog.ShowModal() == wxID_OK) {
            std::ofstream out(saveFileDialog.GetPath().ToStdString());

            // Header
            out << "Line;Word1;Word2\n";

            for (const auto& r : results) {
                out << r.lineNumber << ";" << r.w1 << ";" << r.w2 << "\n";
            }
        }
    }
};

// Drag & Drop implementation
bool FileDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) {
    if (!filenames.empty()) {
        frame->SetFile(filenames[0].ToStdString());
    }
    return true;
}

// ---------- App ----------
class MyApp : public wxApp {
public:
    bool OnInit() override {
        MyFrame* frame = new MyFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);