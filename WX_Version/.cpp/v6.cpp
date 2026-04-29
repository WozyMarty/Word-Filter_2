#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/dnd.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>

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
    JOINTTARGET,
    UNKNOWN
};

struct ParsedTarget {
    TargetType type = TargetType::UNKNOWN;
    std::string name;

    std::string x, y, z;
    std::vector<std::string> joints;
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

bool parseTarget(const std::string& line, ParsedTarget& out) {
    return parseRobtarget(line, out); // only robtarget now
}

// ---------- structured row ----------
struct Row {
    int lineNumber;

    std::string type;
    std::string name;

    std::string x, y, z;
    std::vector<std::string> joints;
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
    wxTextCtrl* filePathCtrl;
    wxTextCtrl* wordCtrl;
    wxTextCtrl* outputCtrl;
    wxChoice* displayChoice;
    wxButton* exportBtn;

    std::string filePath;
    std::vector<std::string> fileCache;
    std::vector<Row> results;

    std::thread worker;
    std::atomic<bool> cancel{ false };

public:
    MyFrame() : wxFrame(nullptr, wxID_ANY,
        "SIM ROBOTICS Edition - Word Search Tool",
        wxDefaultPosition, wxSize(900, 600)) {

        wxPanel* panel = new wxPanel(this);

        wxButton* fileBtn = new wxButton(panel, wxID_ANY, "Select File");
        wxButton* searchBtn = new wxButton(panel, wxID_ANY, "Search");

        filePathCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(500, -1));
        wordCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

        wxString choices[] = {
            "Full line",
            "First 2 words",
            "Robtarget XYZ (compact)"
        };
        displayChoice = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 3, choices);
        displayChoice->SetSelection(0);

        exportBtn = new wxButton(panel, wxID_ANY, "Export CSV");
        exportBtn->Disable();

        outputCtrl = new wxTextCtrl(panel, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY);

        // layout
        wxBoxSizer* main = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* top = new wxBoxSizer(wxHORIZONTAL);
        top->Add(fileBtn, 0, wxRIGHT, 10);
        top->Add(filePathCtrl, 1);

        wxBoxSizer* mid = new wxBoxSizer(wxHORIZONTAL);
        mid->Add(wordCtrl, 1, wxRIGHT, 10);
        mid->Add(searchBtn, 0, wxRIGHT, 10);
        mid->Add(displayChoice, 0, wxRIGHT, 10);
        mid->Add(exportBtn, 0);

        main->Add(top, 0, wxEXPAND | wxALL, 10);
        main->Add(mid, 0, wxEXPAND | wxALL, 10);
        main->Add(outputCtrl, 1, wxEXPAND | wxALL, 10);

        panel->SetSizer(main);

        // events
        fileBtn->Bind(wxEVT_BUTTON, &MyFrame::OnFile, this);
        searchBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSearch, this);
        wordCtrl->Bind(wxEVT_TEXT_ENTER, &MyFrame::OnSearch, this);
        exportBtn->Bind(wxEVT_BUTTON, &MyFrame::OnExport, this);

        SetDropTarget(new FileDropTarget(this));
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
        StartSearch();
    }

    void StartSearch() {
        cancel = true;
        if (worker.joinable()) worker.join();
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

            for (const auto& lineOriginal : fileCache) {
                if (cancel) return;

                std::string line = lineOriginal;
                ++lineNumber;

                if (!containsSubstring(line, word))
                    continue;

                size_t pos = line.find("!");
                if (pos != std::string::npos)
                    line = line.substr(0, pos);

                Row row;
                row.lineNumber = lineNumber;

                std::string display;

                if (choice == 0) {
                    display = std::to_string(lineNumber) + " | " + line;
                    row.type = "text";
                    row.name = line;
                }
                else if (choice == 1) {
                    std::string w1, w2;
                    for (char& c : line) if (c == ',') c = ' ';

                    std::istringstream iss(line);
                    iss >> w1 >> w2;

                    display = std::to_string(lineNumber) + " | " + w1 + " " + w2;

                    row.type = "text";
                    row.name = w1 + " " + w2;
                }
                else if (choice == 2) {
                    ParsedTarget pt;
                    if (!parseTarget(line, pt))
                        continue;

                    row.type = "robtarget";
                    row.name = pt.name;
                    row.x = pt.x;
                    row.y = pt.y;
                    row.z = pt.z;

                    display = std::to_string(lineNumber) + " |   robtarget " + row.name +
                              " X = " + row.x +
                              " Y = " + row.y +
                              " Z = " + row.z;
                }

                local.push_back(row);
                buffer.push_back(display);

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

            if (!buffer.empty()) {
                auto copy = buffer;
                wxTheApp->CallAfter([this, copy]() {
                    for (auto& l : copy)
                        outputCtrl->AppendText(l + "\n");
                });
            }

            wxTheApp->CallAfter([this, local, hits]() {
                results = local;
                outputCtrl->AppendText("\nTotal: " + std::to_string(hits) + "\n");
                if (!results.empty()) exportBtn->Enable();
            });
        });
    }

    void OnExport(wxCommandEvent&) {
        wxFileDialog dlg(this, "Save CSV", "", "",
            "CSV files (*.csv)|*.csv",
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

        if (dlg.ShowModal() == wxID_OK) {
            std::ofstream out(dlg.GetPath().ToStdString());

            out << "Line;Type;Name;X;Y;Z;J1;J2;J3;J4;J5;J6\n";

            for (auto& r : results) {
                out << r.lineNumber << ";"
                    << r.type << ";"
                    << r.name << ";"
                    << r.x << ";"
                    << r.y << ";"
                    << r.z;

                for (size_t i = 0; i < 6; ++i)
                    out << ";";

                out << "\n";
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