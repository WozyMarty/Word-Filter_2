#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/dnd.h>
#include <wx/notebook.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <regex>

// ---------- utils ----------
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

bool containsSubstring(const std::string& line, const std::string& word) {
    return line.find(word) != std::string::npos;
}

// ---------- parsed structure ----------
enum class TargetType {
    ROBTARGET,
    UNKNOWN
};

struct ParsedTarget {
    TargetType type = TargetType::UNKNOWN;
    std::string name;
    std::string x, y, z;
};

// ---------- parsing ----------
bool parseRobtarget(const std::string& line, ParsedTarget& out) {
    if (line.find("robtarget") == std::string::npos)
        return false;

    out.type = TargetType::ROBTARGET;

    size_t start = line.find("robtarget") + 9;
    while (start < line.size() && line[start] == ' ') start++;

    size_t end = line.find(':', start);
    if (end == std::string::npos) return false;

    out.name = trim(line.substr(start, end - start));

    size_t coordStart = line.find("[[", end);
    if (coordStart == std::string::npos) return false;

    coordStart += 2;
    size_t coordEnd = line.find("]", coordStart);
    if (coordEnd == std::string::npos) return false;

    std::string coords = line.substr(coordStart, coordEnd - coordStart);

    std::istringstream iss(coords);
    std::getline(iss, out.x, ',');
    std::getline(iss, out.y, ',');
    std::getline(iss, out.z, ',');

    out.x = trim(out.x);
    out.y = trim(out.y);
    out.z = trim(out.z);

    return true;
}

// ---------- row ----------
struct Row {
    int lineNumber;
    std::string type;
    std::string name;
    std::string x, y, z;
};

// ---------- snapshot ----------
struct Snapshot {
    std::string name;
    wxTextCtrl* ctrl;
};

// forward
class MyFrame;

// ---------- drag & drop ----------
class FileDropTarget : public wxFileDropTarget {
    MyFrame* frame;
public:
    FileDropTarget(MyFrame* f) : frame(f) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& files) override;
};

// ---------- main frame ----------
class MyFrame : public wxFrame {
private:
    // SEARCH
    wxTextCtrl* filePathCtrl;
    wxTextCtrl* wordCtrl;
    wxChoice* displayChoice;
    wxButton* exportBtn;

    wxNotebook* mainNotebook;
    wxNotebook* snapshotNotebook;
    std::vector<Snapshot> snapshots;

    std::string filePath;
    std::vector<std::string> fileCache;
    std::vector<Row> results;

    // COMPARE
    wxChoice* snapA;
    wxChoice* snapB;
    wxTextCtrl* compareOutput;

public:
    MyFrame() : wxFrame(nullptr, wxID_ANY,
        "SIM ROBOTICS - Snapshot Tool (Search + Compare)",
        wxDefaultPosition, wxSize(1000, 650)) {

        wxPanel* root = new wxPanel(this);
        mainNotebook = new wxNotebook(root, wxID_ANY);

        // ================= SEARCH =================
        wxPanel* searchPage = new wxPanel(mainNotebook);

        wxButton* fileBtn = new wxButton(searchPage, wxID_ANY, "Select File");
        wxButton* searchBtn = new wxButton(searchPage, wxID_ANY, "Search");

        filePathCtrl = new wxTextCtrl(searchPage, wxID_ANY, "", wxDefaultPosition, wxSize(500, -1));
        wordCtrl = new wxTextCtrl(searchPage, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

        wxString choices[] = {
            "Full line",
            "First 2 words",
            "Robtarget XYZ"
        };

        displayChoice = new wxChoice(searchPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, 3, choices);
        displayChoice->SetSelection(0);

        exportBtn = new wxButton(searchPage, wxID_ANY, "Export CSV");
        exportBtn->Disable();

        snapshotNotebook = new wxNotebook(searchPage, wxID_ANY);

        wxBoxSizer* searchSizer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* top = new wxBoxSizer(wxHORIZONTAL);
        top->Add(fileBtn, 0, wxRIGHT, 5);
        top->Add(filePathCtrl, 1);

        wxBoxSizer* mid = new wxBoxSizer(wxHORIZONTAL);
        mid->Add(wordCtrl, 1, wxRIGHT, 5);
        mid->Add(searchBtn, 0, wxRIGHT, 5);
        mid->Add(displayChoice, 0, wxRIGHT, 5);
        mid->Add(exportBtn, 0);

        searchSizer->Add(top, 0, wxEXPAND | wxALL, 5);
        searchSizer->Add(mid, 0, wxEXPAND | wxALL, 5);
        searchSizer->Add(snapshotNotebook, 1, wxEXPAND | wxALL, 5);

        searchPage->SetSizer(searchSizer);

        // ================= COMPARE =================
        wxPanel* comparePage = new wxPanel(mainNotebook);

        snapA = new wxChoice(comparePage, wxID_ANY);
        snapB = new wxChoice(comparePage, wxID_ANY);
        wxButton* compareBtn = new wxButton(comparePage, wxID_ANY, "Compare Snapshots");

        compareOutput = new wxTextCtrl(comparePage, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        wxBoxSizer* compareSizer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* compareTop = new wxBoxSizer(wxHORIZONTAL);
        compareTop->Add(snapA, 1, wxRIGHT, 5);
        compareTop->Add(snapB, 1, wxRIGHT, 5);
        compareTop->Add(compareBtn, 0);

        compareSizer->Add(compareTop, 0, wxEXPAND | wxALL, 5);
        compareSizer->Add(compareOutput, 1, wxEXPAND | wxALL, 5);

        comparePage->SetSizer(compareSizer);

        mainNotebook->AddPage(searchPage, "Search");
        mainNotebook->AddPage(comparePage, "Compare");

        wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);
        rootSizer->Add(mainNotebook, 1, wxEXPAND);
        root->SetSizer(rootSizer);

        // events
        fileBtn->Bind(wxEVT_BUTTON, &MyFrame::OnFile, this);
        searchBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSearch, this);
        wordCtrl->Bind(wxEVT_TEXT_ENTER, &MyFrame::OnSearch, this);
        exportBtn->Bind(wxEVT_BUTTON, &MyFrame::OnExport, this);
        compareBtn->Bind(wxEVT_BUTTON, &MyFrame::OnCompare, this);

        SetDropTarget(new FileDropTarget(this));
    }

    wxTextCtrl* CreateSnapshotTab(const std::string& title) {
        wxPanel* page = new wxPanel(snapshotNotebook);

        wxTextCtrl* ctrl = new wxTextCtrl(page, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
        s->Add(ctrl, 1, wxEXPAND);
        page->SetSizer(s);

        snapshotNotebook->AddPage(page, title, true);

        snapshots.push_back({ title, ctrl });
        snapA->Append(title);
        snapB->Append(title);

        return ctrl;
    }

    void LoadFileToCache(const std::string& path) {
        fileCache.clear();
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line))
            fileCache.push_back(line);
    }

    void SetFile(const std::string& path) {
        filePath = path;
        filePathCtrl->SetValue(path);
        LoadFileToCache(path);
    }

    void OnFile(wxCommandEvent&) {
        wxFileDialog dlg(this, "Open file", "", "",
            "Text files (*.txt;*.mod)|*.txt;*.mod|All Files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (dlg.ShowModal() == wxID_OK)
            SetFile(dlg.GetPath().ToStdString());
    }

    void OnSearch(wxCommandEvent&) {
        std::string word = wordCtrl->GetValue().ToStdString();
        if (fileCache.empty() || word.empty()) return;

        results.clear();
        int mode = displayChoice->GetSelection();

        wxTextCtrl* out = CreateSnapshotTab(word);

        for (size_t i = 0; i < fileCache.size(); ++i) {
            const std::string& line = fileCache[i];

            if (!containsSubstring(line, word))
                continue;

            std::string display;

            if (mode == 0) {
                display = std::to_string(i + 1) + " | " + line;
            }
            else if (mode == 1) {
                std::istringstream iss(line);
                std::string w1, w2;
                iss >> w1 >> w2;
                display = std::to_string(i + 1) + " | " + w1 + " " + w2;
            }
            else if (mode == 2) {
                ParsedTarget t;
                if (parseRobtarget(line, t)) {
                    display = std::to_string(i + 1) + " | " +
                        t.name + " | X=" + t.x + " Y=" + t.y + " Z=" + t.z;

                    results.push_back({ (int)i + 1, "robtarget", t.name, t.x, t.y, t.z });
                }
            }

            if (!display.empty())
                out->AppendText(display + "\n");
        }

        if (!results.empty())
            exportBtn->Enable();
    }

    void OnCompare(wxCommandEvent&) {
        int a = snapA->GetSelection();
        int b = snapB->GetSelection();

        if (a == wxNOT_FOUND || b == wxNOT_FOUND || a == b) return;

        std::string textA = snapshots[a].ctrl->GetValue().ToStdString();
        std::string textB = snapshots[b].ctrl->GetValue().ToStdString();

        std::regex removeLineNumber(R"(^\d+\s*\|\s*)", std::regex_constants::multiline);

        textA = std::regex_replace(textA, removeLineNumber, "");
        textB = std::regex_replace(textB, removeLineNumber, "");

        std::regex wordRegex(R"([A-Za-z0-9_]{4,})");

        std::unordered_map<std::string, int> mapA, mapB;

        auto extract = [&](const std::string& text, auto& map) {
            for (std::sregex_iterator i(text.begin(), text.end(), wordRegex), end;
                i != end; ++i) {
                map[i->str()]++;
            }
        };

        extract(textA, mapA);
        extract(textB, mapB);

        compareOutput->Clear();
        compareOutput->AppendText("=== DUPLICATES FOUND ===\n\n");

        for (auto& [k, v] : mapA) {
            if (mapB.count(k)) {
                compareOutput->AppendText("MATCH: " + k + "\n");
            }
        }
    }

    void OnExport(wxCommandEvent&) {
        wxFileDialog dlg(this, "Save CSV", "", "",
            "CSV files (*.csv)|*.csv",
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

        if (dlg.ShowModal() == wxID_OK) {
            std::ofstream out(dlg.GetPath().ToStdString());

            out << "Line;Type;Name;X;Y;Z\n";
            for (auto& r : results) {
                out << r.lineNumber << ";"
                    << r.type << ";"
                    << r.name << ";"
                    << r.x << ";"
                    << r.y << ";"
                    << r.z << "\n";
            }
        }
    }
};

// ---------- DND ----------
bool FileDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& files) {
    if (!files.empty())
        frame->SetFile(files[0].ToStdString());
    return true;
}

// ---------- APP ----------
class MyApp : public wxApp {
public:
    bool OnInit() override {
        MyFrame* f = new MyFrame();
        f->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);