#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/dnd.h>
#include <wx/notebook.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <regex>
#include <algorithm>

// ---------- utils ----------
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end   = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

std::string normalizeSpaces(const std::string& input) {
    std::string out = input;
    std::replace(out.begin(), out.end(), '\t', ' ');
    out = std::regex_replace(out, std::regex(R"(\s+)"), " ");
    return trim(out);
}

std::string replaceCommaWithSpace(std::string input) {
    for (char& c : input) if (c == ',') c = ' ';
    return input;
}

bool containsSubstring(const std::string& line, const std::string& word) {
    return line.find(word) != std::string::npos;
}

std::string csvField(const std::string& val) {
    bool needsQuote = val.find(';') != std::string::npos ||
                      val.find('"') != std::string::npos ||
                      val.find('\n') != std::string::npos;
    if (!needsQuote) return val;
    std::string escaped;
    escaped.reserve(val.size() + 2);
    escaped += '"';
    for (char c : val) {
        if (c == '"') escaped += '"';
        escaped += c;
    }
    escaped += '"';
    return escaped;
}

std::vector<std::string> splitByDelim(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string tok;
    while (std::getline(iss, tok, delim))
        tokens.push_back(tok);
    return tokens;
}

std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

// ---------- parsed structure ----------
struct ParsedTarget {
    std::string name;
    std::string x, y, z;
};

bool parseRobtarget(const std::string& block, ParsedTarget& out) {
    std::string line = normalizeSpaces(block);
    if (line.find("robtarget") == std::string::npos) return false;

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
    iss >> out.x >> out.y >> out.z;
    return true;
}

// ---------- universal result row ----------
struct Row {
    int lineNumber;
    std::vector<std::string> columns;
};

// ---------- snapshot ----------
struct Snapshot {
    std::string name;   // search term
    int         mode;   // display mode used (0=Full,1=First2,2=Robtarget)
    wxTextCtrl* ctrl;
};

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
    wxTextCtrl* filePathCtrl;
    wxTextCtrl* wordCtrl;
    wxChoice*   displayChoice;
    wxButton*   exportBtn;

    wxNotebook* mainNotebook;
    wxNotebook* snapshotNotebook;

    std::vector<Snapshot> snapshots;
    std::vector<std::string> fileCache;

    std::vector<Row> results;
    size_t maxCols = 0;
    int    lastMode = 0;

    wxChoice*   snapA;
    wxChoice*   snapB;
    wxTextCtrl* compareOutput;

public:
    MyFrame();

    void OnFile(wxCommandEvent&);
    void OnSearch(wxCommandEvent&);
    void OnCompare(wxCommandEvent&);
    void OnExport(wxCommandEvent&);

    void LoadFileToCache(const std::string& path);
    void SetFile(const std::string& path);
    wxTextCtrl* CreateSnapshotTab(const std::string& title, int mode);

private:
    // ---------------------------------------------------------------
    // Extract the "target name" from a single display line, depending
    // on the mode that was used when the snapshot was created.
    //
    // Mode 0 (Full line):
    //   "123 | MoveL ToFHRHPickUQ05,v200,fine,..."  -> "ToFHRHPickUQ05"
    //   "123 | CONST robtarget ToFHRHPickUQ05:=[[" -> "ToFHRHPickUQ05"
    //   FANUC block lines are skipped (they never contain a bare name)
    //
    // Mode 1 (First 2 words):
    //   "123 | MoveL ToFHRHPickUQ05"  -> "ToFHRHPickUQ05"
    //   "123 | CONST robtarget"       -> "" (skip — second word is "robtarget", useless)
    //
    // Mode 2 (Robtarget XYZ):
    //   "123 | robtarget ToFHRHPickUQ05 | X=... Y=... Z=..." -> "ToFHRHPickUQ05"
    // ---------------------------------------------------------------
    std::string extractTargetName(const std::string& displayLine, int mode) const
    {
        // Strip leading "NNN | " prefix if present
        std::string line = displayLine;
        {
            static const std::regex prefixRx(R"(^\d+\s*\|\s*)");
            line = std::regex_replace(line, prefixRx, "");
        }
        line = trim(line);
        if (line.empty()) return "";

        if (mode == 2) {
            // "robtarget NAME | X=... Y=... Z=..."
            // Split on " | " and take the second token (index 1)
            auto parts = splitByDelim(line, '|');
            if (parts.size() >= 2) {
                std::string namePart = trim(parts[1]);
                // namePart might be "NAME" directly (no extra info)
                auto words = splitWords(namePart);
                if (!words.empty()) return words[0];
            }
            return "";
        }

        if (mode == 1) {
            // "WORD1 WORD2"  — we want WORD2 (the target name)
            auto words = splitWords(line);
            if (words.size() >= 2) {
                std::string w2 = words[1];
                // Strip trailing comma and anything after it
                size_t comma = w2.find(',');
                if (comma != std::string::npos) w2 = w2.substr(0, comma);
                // Skip if it's just the keyword "robtarget"
                if (w2 == "robtarget") return "";
                return trim(w2);
            }
            return "";
        }

        // mode == 0  (Full line)
        // Case A: Move instruction  ->  "MoveX  TargetName,..."
        {
            static const std::regex moveRx(
                R"(Move[LJCA]\s+([A-Za-z_][A-Za-z0-9_]*))",
                std::regex_constants::icase);
            std::smatch m;
            if (std::regex_search(line, m, moveRx))
                return m[1].str();
        }
        // Case B: MoveAbsJ  ->  skip (joint target, not a robtarget name)
        // Case C: robtarget declaration  ->  "... robtarget NAME :="
        {
            static const std::regex rtRx(
                R"(robtarget\s+([A-Za-z_][A-Za-z0-9_]*)\s*:=)");
            std::smatch m;
            if (std::regex_search(line, m, rtRx))
                return m[1].str();
        }
        return "";
    }

    // Build a set of target names from a snapshot's text output
    std::unordered_map<std::string,bool>
    buildNameSet(int snapIdx) const
    {
        std::unordered_map<std::string,bool> out;
        const Snapshot& snap = snapshots[snapIdx];
        std::string text = snap.ctrl->GetValue().ToStdString();
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            // Skip summary lines
            if (line.find("=== TOTAL") != std::string::npos) continue;
            std::string name = extractTargetName(line, snap.mode);
            if (!name.empty())
                out[name] = true;
        }
        return out;
    }
};

// ---------- constructor ----------
MyFrame::MyFrame() : wxFrame(nullptr, wxID_ANY,
    "SIM ROBOTICS Ed. - Word Filter",
    wxDefaultPosition, wxSize(1000, 650))
{
    SetIcon(wxIcon("ICOS/sim.ico", wxBITMAP_TYPE_ICO));
    wxPanel* root = new wxPanel(this);
    mainNotebook  = new wxNotebook(root, wxID_ANY);

    // SEARCH PAGE
    wxPanel* searchPage = new wxPanel(mainNotebook);

    wxButton* fileBtn   = new wxButton(searchPage, wxID_ANY, "Select File");
    wxButton* searchBtn = new wxButton(searchPage, wxID_ANY, "Search");

    filePathCtrl = new wxTextCtrl(searchPage, wxID_ANY, "", wxDefaultPosition, wxSize(500, -1));
    wordCtrl     = new wxTextCtrl(searchPage, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

    wxString choices[] = { "Full line", "First 2 words", "Robtarget XYZ" };
    displayChoice = new wxChoice(searchPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, 3, choices);
    displayChoice->SetSelection(0);

    exportBtn = new wxButton(searchPage, wxID_ANY, "Export CSV");
    exportBtn->Disable();

    snapshotNotebook = new wxNotebook(searchPage, wxID_ANY);
    snapshotNotebook->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    {
        wxPanel* ph = new wxPanel(snapshotNotebook);
        ph->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        wxStaticText* hint = new wxStaticText(ph, wxID_ANY,
            "Digite um termo e clique em Search para ver os resultados.",
            wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
        wxBoxSizer* phSz = new wxBoxSizer(wxVERTICAL);
        phSz->AddStretchSpacer();
        phSz->Add(hint, 0, wxALIGN_CENTER);
        phSz->AddStretchSpacer();
        ph->SetSizer(phSz);
        snapshotNotebook->AddPage(ph, "...", true);
    }

    wxBoxSizer* s   = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* top = new wxBoxSizer(wxHORIZONTAL);
    top->Add(fileBtn,      0, wxRIGHT, 5);
    top->Add(filePathCtrl, 1);

    wxBoxSizer* mid = new wxBoxSizer(wxHORIZONTAL);
    mid->Add(wordCtrl,      1, wxRIGHT, 5);
    mid->Add(searchBtn,     0, wxRIGHT, 5);
    mid->Add(displayChoice, 0, wxRIGHT, 5);
    mid->Add(exportBtn,     0);

    s->Add(top,              0, wxEXPAND | wxALL, 5);
    s->Add(mid,              0, wxEXPAND | wxALL, 5);
    s->Add(snapshotNotebook, 1, wxEXPAND | wxALL, 5);
    searchPage->SetSizer(s);

    // COMPARE PAGE
    wxPanel* comparePage = new wxPanel(mainNotebook);

    snapA = new wxChoice(comparePage, wxID_ANY);
    snapB = new wxChoice(comparePage, wxID_ANY);
    wxButton* compareBtn = new wxButton(comparePage, wxID_ANY, "Compare");

    compareOutput = new wxTextCtrl(comparePage, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY);

    wxBoxSizer* cs = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* ct = new wxBoxSizer(wxHORIZONTAL);
    ct->Add(snapA,       1, wxRIGHT, 5);
    ct->Add(snapB,       1, wxRIGHT, 5);
    ct->Add(compareBtn,  0);
    cs->Add(ct,           0, wxEXPAND | wxALL, 5);
    cs->Add(compareOutput, 1, wxEXPAND | wxALL, 5);
    comparePage->SetSizer(cs);

    mainNotebook->AddPage(searchPage,  "Search");
    mainNotebook->AddPage(comparePage, "Compare");

    wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);
    rootSizer->Add(mainNotebook, 1, wxEXPAND);
    root->SetSizer(rootSizer);

    fileBtn->Bind(wxEVT_BUTTON,      &MyFrame::OnFile,    this);
    searchBtn->Bind(wxEVT_BUTTON,    &MyFrame::OnSearch,  this);
    wordCtrl->Bind(wxEVT_TEXT_ENTER, &MyFrame::OnSearch,  this);
    exportBtn->Bind(wxEVT_BUTTON,    &MyFrame::OnExport,  this);
    compareBtn->Bind(wxEVT_BUTTON,   &MyFrame::OnCompare, this);

    SetDropTarget(new FileDropTarget(this));
}

// ---------- file ----------
void MyFrame::LoadFileToCache(const std::string& path) {
    fileCache.clear();
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
        fileCache.push_back(line);
}

void MyFrame::SetFile(const std::string& path) {
    filePathCtrl->SetValue(path);
    LoadFileToCache(path);
}

void MyFrame::OnFile(wxCommandEvent&) {
    wxFileDialog dlg(this, "Open file", "", "", "*.*", wxFD_OPEN);
    if (dlg.ShowModal() == wxID_OK)
        SetFile(dlg.GetPath().ToStdString());
}

// ---------- snapshot tab ----------
wxTextCtrl* MyFrame::CreateSnapshotTab(const std::string& title, int mode) {
    wxPanel* page = new wxPanel(snapshotNotebook);
    page->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    wxTextCtrl* ctrl = new wxTextCtrl(page, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);

    ctrl->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    wxBoxSizer* sz = new wxBoxSizer(wxVERTICAL);
    sz->Add(ctrl, 1, wxEXPAND);
    page->SetSizer(sz);

    snapshotNotebook->AddPage(page, title, true);

    CallAfter([page, ctrl]() {
        page->Layout();
        page->Refresh();
        page->Update();
        ctrl->Refresh();
        ctrl->Update();
    });

    snapshots.push_back({ title, mode, ctrl });
    snapA->Append(title);
    snapB->Append(title);

    return ctrl;
}

// ---------- search ----------
void MyFrame::OnSearch(wxCommandEvent&) {
    std::string word = wordCtrl->GetValue().ToStdString();
    if (fileCache.empty() || word.empty()) return;

    results.clear();
    maxCols  = 0;
    lastMode = displayChoice->GetSelection();

    if (snapshotNotebook->GetPageCount() > 0 &&
        snapshotNotebook->GetPageText(0) == "...")
    {
        snapshotNotebook->DeletePage(0);
    }

    wxTextCtrl* out = CreateSnapshotTab(word, lastMode);
    int hitCount = 0;

    static const std::regex pBlockRx(R"(P\[\d+:"[^"]*"\]\s*\{)");

    for (size_t i = 0; i < fileCache.size(); ++i) {
        const std::string& line      = fileCache[i];
        std::string        lineClean = replaceCommaWithSpace(line);

        if (!containsSubstring(line, word)) continue;
        hitCount++;

        std::string display;
        Row         row;
        row.lineNumber = (int)i + 1;

        if (lastMode == 0) {
            if (std::regex_search(line, pBlockRx)) {
                std::string block = line;
                size_t j = i;
                while (j + 1 < fileCache.size()) {
                    const std::string& next = fileCache[++j];
                    block += "\n" + next;
                    if (next.find("};") != std::string::npos) break;
                }
                i = j;
                display = block;
                row.columns.push_back(block);
            }
            else {
                display = std::to_string(i + 1) + " | " + line;
                std::vector<std::string> parts = splitByDelim(line, ',');
                for (auto& p : parts) {
                    std::vector<std::string> words = splitWords(p);
                    for (auto& w : words)
                        row.columns.push_back(trim(w));
                }
                if (row.columns.empty())
                    row.columns = splitWords(lineClean);
            }
        }
        else if (lastMode == 1) {
            std::istringstream iss(lineClean);
            std::string w1, w2;
            iss >> w1 >> w2;
            display = std::to_string(i + 1) + " | " + w1 + " " + w2;
            if (!w1.empty()) row.columns.push_back(w1);
            if (!w2.empty()) row.columns.push_back(w2);
        }
        else {
            std::string block      = line;
            std::string blockClean = lineClean;
            size_t j = i;

            while (block.find("];") == std::string::npos && j + 1 < fileCache.size()) {
                j++;
                block      += " " + fileCache[j];
                blockClean += " " + replaceCommaWithSpace(fileCache[j]);
            }

            ParsedTarget t;
            if (parseRobtarget(blockClean, t)) {
                display = std::to_string(i + 1) + " | robtarget " +
                          t.name + " | X=" + t.x + " Y=" + t.y + " Z=" + t.z;
                row.columns = { "robtarget", t.name, t.x, t.y, t.z };
            }
            i = j;
        }

        if (!display.empty())
            out->AppendText(display + "\n");

        if (!row.columns.empty()) {
            maxCols = std::max(maxCols, row.columns.size());
            results.push_back(std::move(row));
        }
    }

    out->AppendText("\n=== TOTAL HITS: " + std::to_string(hitCount) + " ===\n");
    exportBtn->Enable(!results.empty());
}

// ---------- compare ----------
// Logic:
//   Left  snapshot (snapA) = reference list  (e.g. Move instructions)
//   Right snapshot (snapB) = definition list (e.g. robtarget declarations)
//
//   For every name found in LEFT, check whether it exists in RIGHT.
//   Report OK / MISSING accordingly.
//
//   extractTargetName() is mode-aware, so the comparison works regardless
//   of which mode each snapshot was created with.
void MyFrame::OnCompare(wxCommandEvent&) {
    int a = snapA->GetSelection();
    int b = snapB->GetSelection();
    if (a == wxNOT_FOUND || b == wxNOT_FOUND || a == b) {
        compareOutput->Clear();
        compareOutput->AppendText("Selecione dois snapshots diferentes.\n");
        return;
    }

    auto leftNames  = buildNameSet(a);
    auto rightNames = buildNameSet(b);

    compareOutput->Clear();
    int ok = 0, missing = 0;

    // Sort keys for stable output
    std::vector<std::string> keys;
    keys.reserve(leftNames.size());
    for (auto& [k, _] : leftNames) keys.push_back(k);
    std::sort(keys.begin(), keys.end());

    for (const auto& name : keys) {
        if (rightNames.count(name)) {
            compareOutput->AppendText("OK:      " + name + "\n");
            ok++;
        } else {
            compareOutput->AppendText("MISSING: " + name + "\n");
            missing++;
        }
    }

    compareOutput->AppendText(
        "\n=== TOTAL: " + std::to_string(ok + missing) +
        " | OK: "      + std::to_string(ok) +
        " | MISSING: " + std::to_string(missing) + " ===\n");
}

// ---------- export CSV ----------
void MyFrame::OnExport(wxCommandEvent&) {
    if (results.empty()) return;

    wxFileDialog dlg(this, "Save CSV", "", "",
        "CSV files (*.csv)|*.csv",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) return;

    std::ofstream out(dlg.GetPath().ToStdString());

    out << "Line";
    if (lastMode == 0) {
        for (size_t c = 1; c <= maxCols; ++c)
            out << ";Col_" << c;
    } else if (lastMode == 1) {
        out << ";Word_1;Word_2";
    } else {
        out << ";Type;Name;X;Y;Z";
    }
    out << "\n";

    for (const auto& r : results) {
        out << r.lineNumber;
        for (const auto& col : r.columns)
            out << ";" << csvField(col);
        for (size_t c = r.columns.size(); c < maxCols; ++c)
            out << ";";
        out << "\n";
    }
}

// ---------- DND ----------
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