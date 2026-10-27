#include "cps/Preprocessor.h"
#include "cps/Diagnostics.h"
#include <string_view>
#include <unordered_set>

using namespace cps;

namespace {
bool nameStart(char C) {
    return (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_';
}

bool nameContinue(char C) {
    return nameStart(C) || (C >= '0' && C <= '9');
}

std::string_view trim(std::string_view Text) {
    const auto First = Text.find_first_not_of(" \t\r\v\f");
    if (First == std::string_view::npos) return {};
    return Text.substr(First, Text.find_last_not_of(" \t\r\v\f") - First + 1);
}
}

PreprocessedSource cps::preprocess(std::istream &Input) {
    PreprocessedSource Result;
    std::unordered_set<std::string> Switches;
    struct Conditional { bool ParentActive; int Line; };
    std::vector<Conditional> Conditions;
    bool Active = true;
    int LineNumber = 0;
    auto Error = [&](const char *Message) {
        sourceError(LineNumber, "%s", Message);
        Result.HadError = true;
    };
    std::string Line;
    while (std::getline(Input, Line)) {
        ++LineNumber;
        Result.LibraryLines.push_back(Switches.count("__LIB") != 0);
        auto Code = trim(Line);
        size_t WordEnd = 0;
        while (WordEnd < Code.size() && nameContinue(Code[WordEnd])) ++WordEnd;
        auto Word = Code.substr(0, WordEnd);
        bool Define = Word == "PP_DEFINE", Undef = Word == "PP_UNDEF";
        bool Ifdef = Word == "PP_IFDEF", Ifndef = Word == "PP_IFNDEF";
        bool Endif = Word == "PP_ENDIF";
        bool Directive = Define || Undef || Ifdef || Ifndef || Endif;
        bool Mask = Directive || !Active;
        if (Directive) {
            auto Tail = Code.substr(WordEnd);
            Tail = trim(Tail.substr(0, Tail.find("//")));
            if (Endif) {
                if (!Tail.empty()) Error("PP_ENDIF does not take arguments");
                if (Conditions.empty()) Error("PP_ENDIF without matching PP_IFDEF or PP_IFNDEF");
                else {
                    Active = Conditions.back().ParentActive;
                    Conditions.pop_back();
                }
            } else if (Ifdef || Ifndef || Active) {
                size_t NameEnd = 0;
                if (!Tail.empty() && nameStart(Tail[0])) {
                    NameEnd = 1;
                    while (NameEnd < Tail.size() && nameContinue(Tail[NameEnd])) ++NameEnd;
                }
                bool Valid = NameEnd != 0 && NameEnd == Tail.size();
                if (!Valid) {
                    sourceError(LineNumber, "%.*s requires exactly one switch name",
                                static_cast<int>(Word.size()), Word.data());
                    Result.HadError = true;
                }
                std::string Name(Tail.substr(0, NameEnd));
                if (Ifdef || Ifndef) {
                    Conditions.push_back({Active, LineNumber});
                    bool Defined = Switches.count(Name) != 0;
                    Active = Active && Valid && (Ifdef ? Defined : !Defined);
                } else if (Valid) {
                    if (Define) Switches.insert(Name);
                    else Switches.erase(Name);
                }
            }
        }
        if (Mask)
            for (char &C : Line)
                if (C != '\r') C = ' ';
        Result.Text += Line;
        if (!Input.eof()) Result.Text += '\n';
    }
    for (const auto &Condition : Conditions) {
        sourceError(Condition.Line, "Unterminated preprocessor conditional: expected PP_ENDIF");
        Result.HadError = true;
    }
    return Result;
}
