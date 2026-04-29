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

// Escape a field for CSV (semicolon-separated).
// Wraps in quotes if the value contains ; " or newline.
std::string csvField(const std::string& val) {
    bool needsQuote = val.find(';') != std::string::npos ||
                      val.find('"') != std::string::npos ||
                      val.find('\n') != std::string::npos;
    if (!needsQuote) return val;
    std::string escaped;
    escaped.reserve(val.size() + 2);
    escaped += '"';
    for (char c : val) {
        if (c == '"') escaped += '"'; // double the quote
        escaped += c;
    }
    escaped += '"';
    return escaped;
}

// Split a string by a delimiter into tokens (no empty tokens skipped).
std::vector<std::string> splitByDelim(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string tok;
    while (std::getline(iss, tok, delim))
        tokens.push_back(tok);
    return tokens;
}

// Split on whitespace (skips multiple spaces).
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
// lineNumber  : 1-based line in file
// columns     : dynamic list of values for this hit
// maxCols     : updated globally so CSV header is wide enough
struct Row {
    int lineNumber;
    std::vector<std::string> columns; // col[0] = first token, col[1] = second, ...
};

// ---------- snapshot ----------
struct Snapshot {
    std::string name;
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

    // universal results
    std::vector<Row> results;
    size_t maxCols = 0;   // widest row seen during last search
    int    lastMode = 0;  // display mode used during last search

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
    wxTextCtrl* CreateSnapshotTab(const std::string& title);
};

// ---------- constructor ----------
MyFrame::MyFrame() : wxFrame(nullptr, wxID_ANY,
    "SIM ROBOTICS - Snapshot Tool",
    wxDefaultPosition, wxSize(1000, 650))
{
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

    fileBtn->Bind(wxEVT_BUTTON,      &MyFrame::OnFile,   this);
    searchBtn->Bind(wxEVT_BUTTON,    &MyFrame::OnSearch, this);
    wordCtrl->Bind(wxEVT_TEXT_ENTER, &MyFrame::OnSearch, this);
    exportBtn->Bind(wxEVT_BUTTON,    &MyFrame::OnExport, this);
    compareBtn->Bind(wxEVT_BUTTON,   &MyFrame::OnCompare,this);

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
wxTextCtrl* MyFrame::CreateSnapshotTab(const std::string& title) {
    wxPanel* page = new wxPanel(snapshotNotebook);
    wxTextCtrl* ctrl = new wxTextCtrl(page, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY);

    wxBoxSizer* sz = new wxBoxSizer(wxVERTICAL);
    sz->Add(ctrl, 1, wxEXPAND);
    page->SetSizer(sz);

    snapshotNotebook->AddPage(page, title, true);

    snapshots.push_back({ title, ctrl });
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

    wxTextCtrl* out = CreateSnapshotTab(word);
    int hitCount = 0;

    // Regex that matches FANUC-style point definition lines:
    // e.g.  P[4:"GSC2B165397"]{
    //       P[10:"GSC2B157299"]{
    // The number and label inside the quotes can vary freely.
    static const std::regex pBlockRx(R"(P\[\d+:"[^"]*"\]\s*\{)");

    for (size_t i = 0; i < fileCache.size(); ++i) {
        const std::string& line      = fileCache[i];
        std::string        lineClean = replaceCommaWithSpace(line);

        if (!containsSubstring(line, word)) continue;
        hitCount++;

        std::string display;
        Row         row;
        row.lineNumber = (int)i + 1;

        // -------------------------------------------------------
        // MODE 0 — Full line
        // -------------------------------------------------------
        if (lastMode == 0) {

            // ── Detect a P[N:"..."]{ block and collect until }; ──
            if (std::regex_search(line, pBlockRx)) {
                // Accumulate lines until we hit the closing };
                std::string block = line;
                size_t j = i;
                while (j + 1 < fileCache.size()) {
                    const std::string& next = fileCache[++j];
                    block += "\n" + next;
                    // Stop after the line that closes the block
                    if (next.find("};") != std::string::npos) break;
                }
                i = j; // skip the lines we already consumed

                display = block;               // show the whole block as-is
                row.columns.push_back(block);  // single column for CSV
            }
            else {
                // Normal full-line handling
                display = std::to_string(i + 1) + " | " + line;

                // Split original line by comma first, then trim spaces
                std::vector<std::string> parts = splitByDelim(line, ',');
                for (auto& p : parts) {
                    std::vector<std::string> words = splitWords(p);
                    for (auto& w : words)
                        row.columns.push_back(trim(w));
                }
                // If no comma at all, fall back to whitespace split
                if (row.columns.empty())
                    row.columns = splitWords(lineClean);
            }
        }
        // -------------------------------------------------------
        // MODE 1 — First 2 words
        // Columns: word1, word2
        // -------------------------------------------------------
        else if (lastMode == 1) {
            std::istringstream iss(lineClean);
            std::string w1, w2;
            iss >> w1 >> w2;
            display = std::to_string(i + 1) + " | " + w1 + " " + w2;
            if (!w1.empty()) row.columns.push_back(w1);
            if (!w2.empty()) row.columns.push_back(w2);
        }
        // -------------------------------------------------------
        // MODE 2 — Robtarget XYZ
        // Columns: Type, Name, X, Y, Z
        // -------------------------------------------------------
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
void MyFrame::OnCompare(wxCommandEvent&) {
    int a = snapA->GetSelection();
    int b = snapB->GetSelection();
    if (a == wxNOT_FOUND || b == wxNOT_FOUND || a == b) return;

    std::string textA = snapshots[a].ctrl->GetValue().ToStdString();
    std::string textB = snapshots[b].ctrl->GetValue().ToStdString();

    std::regex removeLine(R"(^\d+\s*\|\s*)", std::regex_constants::multiline);
    textA = std::regex_replace(textA, removeLine, "");
    textB = std::regex_replace(textB, removeLine, "");

    std::unordered_map<std::string, bool> leftEntries, rightEntries;

    auto extractSecondWord = [](const std::string& text,
                                std::unordered_map<std::string, bool>& outMap) {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            line = replaceCommaWithSpace(line);
            line = normalizeSpaces(line);
            std::istringstream ls(line);
            std::string w1, w2;
            if (!(ls >> w1)) continue;
            if (!(ls >> w2)) continue;
            std::string key = std::regex_replace(w2, std::regex(R"([^A-Za-z0-9_])"), "");
            if (!key.empty()) outMap[key] = true;
        }
    };

    extractSecondWord(textA, leftEntries);
    extractSecondWord(textB, rightEntries);

    compareOutput->Clear();
    int ok = 0, missing = 0;

    for (auto& [entry, _] : leftEntries) {
        if (rightEntries.count(entry)) {
            compareOutput->AppendText("OK: " + entry + "\n");
            ok++;
        } else {
            compareOutput->AppendText("MISSING: " + entry + "\n");
            missing++;
        }
    }

    compareOutput->AppendText("\n=== TOTAL: " + std::to_string(ok + missing) +
        " | OK: " + std::to_string(ok) +
        " | MISSING: " + std::to_string(missing) + " ===\n");
}

// ---------- export CSV ----------
// Header is built dynamically based on the mode that was used and the
// widest row found. Each token from the source line becomes its own column.
void MyFrame::OnExport(wxCommandEvent&) {
    if (results.empty()) return;

    wxFileDialog dlg(this, "Save CSV", "", "",
        "CSV files (*.csv)|*.csv",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) return;

    std::ofstream out(dlg.GetPath().ToStdString());

    // ---- Build header ----
    out << "Line";

    if (lastMode == 0) {
        // Dynamic: Col_1 .. Col_N  (N = widest row)
        for (size_t c = 1; c <= maxCols; ++c)
            out << ";Col_" << c;
    } else if (lastMode == 1) {
        out << ";Word_1;Word_2";
    } else {
        out << ";Type;Name;X;Y;Z";
    }
    out << "\n";

    // ---- Data rows ----
    for (const auto& r : results) {
        out << r.lineNumber;
        for (const auto& col : r.columns)
            out << ";" << csvField(col);
        // Pad missing columns with empty fields so every row has the same width
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