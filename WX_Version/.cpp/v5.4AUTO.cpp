#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/dnd.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>

// ---------- substring search ----------
bool containsSubstring(const std::string& line, const std::string& word) {
    return line.find(word) != std::string::npos;
}

// ---------- data ----------
struct Row {
    int lineNumber;
    std::string w1;
    std::string w2;
};

// ---------- forward ----------
class MyFrame;

// ---------- drag & drop ----------
class FileDropTarget : public wxFileDropTarget {
private:
    MyFrame* frame;

public:
    FileDropTarget(MyFrame* f) : frame(f) {}

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& files) override;
};

// ---------- main frame ----------
class MyFrame : public wxFrame {
private:
    wxTextCtrl* filePathCtrl;
    wxTextCtrl* wordCtrl;
    wxTextCtrl* outputCtrl;
    wxChoice* displayChoice;
    wxButton* exportBtn;

    wxTimer searchTimer;

    std::string filePath;

    // 🔥 CACHE FILE IN MEMORY (IMPORTANT FIX)
    std::vector<std::string> fileCache;

    std::vector<Row> results;

    std::thread worker;
    std::atomic<bool> cancel{ false };

public:
    MyFrame()
        : wxFrame(nullptr, wxID_ANY, "Fast Cached Search Tool", wxDefaultPosition, wxSize(850, 600)),
          searchTimer(this) {

        wxPanel* panel = new wxPanel(this);

        wxButton* fileBtn = new wxButton(panel, wxID_ANY, "Select File");

        filePathCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(500, -1));

        wxStaticText* label = new wxStaticText(panel, wxID_ANY, "Search:");
        wordCtrl = new wxTextCtrl(panel, wxID_ANY);

        wxString choices[] = { "Full line", "First 2 words" };
        displayChoice = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 2, choices);
        displayChoice->SetSelection(0);

        exportBtn = new wxButton(panel, wxID_ANY, "Export CSV");
        exportBtn->Disable();

        outputCtrl = new wxTextCtrl(panel, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        // ---------- layout ----------
        wxBoxSizer* main = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* top = new wxBoxSizer(wxHORIZONTAL);
        top->Add(fileBtn, 0, wxRIGHT, 10);
        top->Add(filePathCtrl, 1);

        wxBoxSizer* mid = new wxBoxSizer(wxHORIZONTAL);
        mid->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        mid->Add(wordCtrl, 1, wxRIGHT, 10);
        mid->Add(displayChoice, 0, wxRIGHT, 10);
        mid->Add(exportBtn, 0);

        main->Add(top, 0, wxEXPAND | wxALL, 10);
        main->Add(mid, 0, wxEXPAND | wxALL, 10);
        main->Add(outputCtrl, 1, wxEXPAND | wxALL, 10);

        panel->SetSizer(main);

        // ---------- events ----------
        fileBtn->Bind(wxEVT_BUTTON, &MyFrame::OnFile, this);

        wordCtrl->Bind(wxEVT_TEXT, &MyFrame::OnTyping, this);
        displayChoice->Bind(wxEVT_CHOICE, &MyFrame::OnTyping, this);

        exportBtn->Bind(wxEVT_BUTTON, &MyFrame::OnExport, this);

        Bind(wxEVT_TIMER, &MyFrame::OnTimer, this);

        this->SetDropTarget(new FileDropTarget(this));
    }

    // ---------- LOAD FILE ONLY ONCE ----------
    void LoadFileToCache(const std::string& path) {
        fileCache.clear();

        std::ifstream in(path);
        std::string line;

        while (std::getline(in, line)) {
            fileCache.push_back(line);
        }
    }

    // ---------- file ----------
    void SetFile(const std::string& path) {
        filePath = path;
        filePathCtrl->SetValue(path);

        LoadFileToCache(path);   // 🔥 IMPORTANT FIX
        StartSearch();
    }

    void OnFile(wxCommandEvent&) {
        wxFileDialog dlg(
            this,
            "Open file",
            "",
            "",
            "Module Files (*.txt;*.mod)|*.txt;*.mod|All Files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST
        );

        if (dlg.ShowModal() == wxID_OK) {
            SetFile(dlg.GetPath().ToStdString());
        }
    }

    // ---------- debounce ----------
    void OnTyping(wxCommandEvent&) {
        searchTimer.Stop();
        searchTimer.Start(1000, wxTIMER_ONE_SHOT); // 1 sec
    }

    void OnTimer(wxTimerEvent&) {
        StartSearch();
    }

    // ---------- SEARCH (FAST MEMORY VERSION) ----------
    void StartSearch() {

        cancel = true;
        if (worker.joinable())
            worker.join();

        cancel = false;

        std::string word = wordCtrl->GetValue().ToStdString();
        int choice = displayChoice->GetSelection();

        outputCtrl->Clear();
        results.clear();
        exportBtn->Disable();

        if (fileCache.empty() || word.empty()) return;

        worker = std::thread([=]() {

            int lineNumber = 0;
            int hits = 0;

            std::vector<std::string> buffer;
            std::vector<Row> local;

            buffer.reserve(100);

            for (const auto& lineOriginal : fileCache) {

                if (cancel) return;

                std::string line = lineOriginal;

                ++lineNumber;

                if (!containsSubstring(line, word))
                    continue;

                size_t pos = line.find("!");
                if (pos != std::string::npos)
                    line = line.substr(0, pos);

                std::string w1, w2;

                if (choice == 0) {
                    w1 = line;
                    w2 = "";
                } else {
                    for (char &c : line)
                        if (c == ',') c = ' ';

                    std::istringstream iss(line);
                    iss >> w1 >> w2;
                }

                local.push_back({ lineNumber, w1, w2 });

                std::string display =
                    std::to_string(lineNumber) + " | " + w1 +
                    (w2.empty() ? "" : " " + w2);

                buffer.push_back(display);

                // 🔥 batch UI updates
                if (buffer.size() >= 100) {
                    auto copy = buffer;
                    buffer.clear();

                    wxTheApp->CallAfter([this, copy]() {
                        for (auto& l : copy)
                            outputCtrl->AppendText(l + "\n");
                    });
                }

                hits++;
            }

            // final flush
            if (!buffer.empty()) {
                auto copy = buffer;

                wxTheApp->CallAfter([this, copy]() {
                    for (auto& l : copy)
                        outputCtrl->AppendText(l + "\n");
                });
            }

            wxTheApp->CallAfter([this, local, hits]() mutable {
                results = local;
                outputCtrl->AppendText("\nTotal: " + std::to_string(hits) + "\n");
                if (!results.empty()) exportBtn->Enable();
            });
        });
    }

    // ---------- export ----------
    void OnExport(wxCommandEvent&) {
        wxFileDialog dlg(
            this,
            "Save CSV",
            "",
            "",
            "CSV files (*.csv)|*.csv",
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT
        );

        if (dlg.ShowModal() == wxID_OK) {
            std::ofstream out(dlg.GetPath().ToStdString());

            out << "Line;Word1;Word2\n";
            for (auto& r : results) {
                out << r.lineNumber << ";" << r.w1 << ";" << r.w2 << "\n";
            }
        }
    }
};

// ---------- drag & drop ----------
bool FileDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& files) {
    if (!files.empty())
        frame->SetFile(files[0].ToStdString());
    return true;
}

// ---------- app ----------
class MyApp : public wxApp {
public:
    bool OnInit() override {
        MyFrame* f = new MyFrame();
        f->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);