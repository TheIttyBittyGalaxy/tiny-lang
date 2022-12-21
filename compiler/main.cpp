#include <string.h>
#include <string>
#include <iomanip>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::vector;

// ERROR HANDLING //
bool error_has_occoured = false;

// TOKENS //

enum class TokenKind
{
    Null,

    Identity,
    CurlyL,
    CurlyR,
    ParenL,
    ParenR,

    Comma,

    String,

    InsertL,
    InsertR,
    LessThan,
    LessThanEqual,
    GreaterThan,
    GreaterThanEqual,

    Line,
    EndOfFile,
};

struct Token
{
    TokenKind kind;
    string str;
    int line;

    Token() : kind(TokenKind::Null),
              str(""),
              line(0) {}

    Token(TokenKind kind, string str, int line) : kind(kind),
                                                  str(str),
                                                  line(line) {}
};

// LEXER //

struct Lexer
{
    string *src;
    size_t token_pos = 0;
    size_t current_pos = 0;
    size_t current_line = 0;
    bool finished = false;
    vector<Token> tokens;

    Lexer(string *src) : src(src) {}
};

void error(const Lexer &lexer, const string msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    cout << "Error on line " << lexer.current_line << ": " << msg << endl;
}

char peek(const Lexer &lexer)
{
    return (*lexer.src)[lexer.current_pos];
}

char next(Lexer &lexer)
{
    char c = peek(lexer);
    lexer.current_pos++;
    if (c == '\n')
        lexer.current_line++;
    return c;
}

bool match(Lexer &lexer, const char c)
{
    if (peek(lexer) != c)
        return false;
    next(lexer);
    return true;
}

bool char_is_name(const char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void make_token(Lexer &lexer, const TokenKind kind)
{
    Token t;
    t.kind = kind;
    size_t length = (lexer.current_pos - lexer.token_pos);
    t.str = lexer.src->substr(lexer.token_pos, length);
    t.line = kind != TokenKind::Line ? lexer.current_line : lexer.current_line - 1;

    lexer.tokens.push_back(t);

    cout
        << std::setfill('0') << std::setw(2) << (int)t.kind << " "
        << ((t.kind == TokenKind::Line)
                ? "new line"
                : t.str)
        << endl;
}

void next_token(Lexer &lexer)
{
    if (lexer.finished)
        return;

    while (peek(lexer) == ' ' || peek(lexer) == '\t')
        next(lexer);

    lexer.token_pos = lexer.current_pos;

    if (peek(lexer) == '\0')
    {
        make_token(lexer, TokenKind::EndOfFile);
        lexer.finished = true;
        return;
    }

    char c = next(lexer);
    switch (c)
    {
    case '\n':
        make_token(lexer, TokenKind::Line);
        return;

    case '(':
        make_token(lexer, TokenKind::ParenL);
        return;
    case ')':
        make_token(lexer, TokenKind::ParenR);
        return;
    case '{':
        make_token(lexer, TokenKind::CurlyL);
        return;
    case '}':
        make_token(lexer, TokenKind::CurlyR);
        return;

    case ',':
        make_token(lexer, TokenKind::Comma);
        return;

    case '<':
    {
        switch (peek(lexer))
        {
        case '<':
            next(lexer);
            make_token(lexer, TokenKind::InsertL);
            return;
        case '=':
            next(lexer);
            make_token(lexer, TokenKind::LessThanEqual);
            return;
        }
        make_token(lexer, TokenKind::LessThan);
        return;
    }

    case '>':
    {
        switch (peek(lexer))
        {
        case '>':
            next(lexer);
            make_token(lexer, TokenKind::InsertR);
            return;
        case '=':
            next(lexer);
            make_token(lexer, TokenKind::GreaterThanEqual);
            return;
        }
        make_token(lexer, TokenKind::GreaterThan);
        return;
    }

    case '"':
    {
        char n;
        do
        {
            n = next(lexer);
            if (n == '\0')
                error(lexer, "Unterminated string at end of file");
        } while (n != '"');
        make_token(lexer, TokenKind::String);
        return;
    }

    default:
    {
        if (char_is_name(c))
        {
            while (char_is_name(peek(lexer)))
                next(lexer);
            make_token(lexer, TokenKind::Identity);
            return;
        }

        error(lexer, "Unable to parse character '" + string(1, c) + "'");
        return;
    }
    }
}

// PROGRAM MODEL //

enum class DeclarableKind
{
    Null,
    Variable,
    Function,
};

struct Declarable
{
    DeclarableKind kind = DeclarableKind::Null;
    string identity = "UNDEFINED";
};

struct Scope
{
    std::map<string, Declarable> declarables;
    Scope *parent = nullptr;

    Scope(){};
    Scope(Scope *parent) : parent(parent){};
};

Declarable *fetch(Scope *scope, string id)
{
    auto got = scope->declarables.find(id);
    if (got != scope->declarables.end())
        return &got->second;

    if (scope->parent != nullptr)
        return fetch(scope->parent, id);

    return nullptr;
}

void declare(Scope &scope, string id, DeclarableKind kind)
{
    Declarable dec;
    dec.identity = id;
    dec.kind = kind;
    scope.declarables[id] = dec;
}

// COMPILER //

struct Compiler
{
    Lexer *lexer;
    size_t current_token = 0;

    stringstream *out = nullptr;
    stringstream *out_statement_block = nullptr;

    bool insert_stmt = false;
    bool inserting_chars = false;
    bool inserting_ltr = false;
    bool in_main = false;

    Compiler(Lexer *lexer) : lexer(lexer) {}
};

Token current_token(const Compiler &compiler)
{
    size_t i = std::min(compiler.current_token, compiler.lexer->tokens.size() - 1);
    return compiler.lexer->tokens.at(i);
}

TokenKind peek(const Compiler &compiler)
{
    return current_token(compiler).kind;
}

TokenKind peek_ahead(const Compiler &compiler, size_t amt)
{
    size_t i = std::min(compiler.current_token + amt, compiler.lexer->tokens.size() - 1);
    return compiler.lexer->tokens.at(i).kind;
}

void error(const Compiler &compiler, const string msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    Token current = current_token(compiler);
    cout << "Error on line " << current.line << ": " << msg;
    if (current.kind == TokenKind::Line)
        cout << " (got new line)";
    else
        cout << " (got " << std::setfill('0') << std::setw(2) << (int)current.kind << " " << current.str << ")";
    cout << endl;
}

bool match(Compiler &compiler, TokenKind kind)
{
    if (peek(compiler) != kind)
        return false;
    next_token(*compiler.lexer);
    compiler.current_token++;
    return true;
}

void skip_lines(Compiler &compiler)
{
    while (match(compiler, TokenKind::Line))
        ;
}

Token eat(Compiler &compiler, TokenKind kind, const string msg)
{
    if (kind != TokenKind::Line)
    {
        skip_lines(compiler);
    }

    Token t = current_token(compiler);
    if (!match(compiler, kind))
        error(compiler, msg);
    return t;
}

bool peek_call(const Compiler &compiler)
{
    return peek(compiler) == TokenKind::Identity && peek_ahead(compiler, 1) == TokenKind::ParenL;
}

bool peek_expression(const Compiler &compiler)
{
    return peek_call(compiler) ||
           peek(compiler) == TokenKind::String ||
           peek(compiler) == TokenKind::Identity;
}

void compile_expression(Compiler &compiler, Scope &scope);

void compile_identity(Compiler &compiler, Scope &scope)
{
    Token identity = eat(compiler, TokenKind::Identity, "Expected identity.");
    if (identity.str == "console")
    {
        *compiler.out << (compiler.inserting_ltr ? "std::cin" : "std::cout");

        // FIXME: This should only occour if it happens in the first expression of the insert statement
        if (!compiler.inserting_ltr)
            compiler.inserting_chars = true;
    }
    else
    {
        string id = identity.str;
        Declarable *dec = fetch(&scope, id);

        if (dec == nullptr)
        {
            *compiler.out_statement_block << "int " << id << ";";
            declare(scope, id, DeclarableKind::Variable);
        }
        else if (dec->kind == DeclarableKind::Function)
        {
            error(compiler, "Cannot use function as an expression.");
        }

        *compiler.out << id;
    }
}

void compile_call(Compiler &compiler, Scope &scope)
{
    string id = eat(compiler, TokenKind::Identity, "Expected function name.").str;
    Declarable *funct = fetch(&scope, id);

    if (funct == nullptr)
        error(compiler, "Function '" + id + "' does not exist.");
    else if (funct->kind != DeclarableKind::Function)
        error(compiler, "'" + id + "' is not a function.");

    eat(compiler, TokenKind::ParenL, "Expected '(' after function name.");
    *compiler.out << id << "(";

    if (peek_expression(compiler))
    {
        compile_expression(compiler, scope);
        while (match(compiler, TokenKind::Comma))
        {
            *compiler.out << ",";
            compile_expression(compiler, scope);
        }
    }

    eat(compiler, TokenKind::ParenR, "Expected ')' after function arguments.");
    *compiler.out << ")";
}

void compile_list_literal(Compiler &compiler)
{
    vector<int> values;
    if (peek(compiler) == TokenKind::String)
    {
        Token token = eat(compiler, TokenKind::String, "Expected string.");
        for (int i = 1; i <= token.str.length() - 2; i++)
            values.push_back((int)token.str[i]);
    }
    else
    {
        // TODO: Parse array literals
    }

    if (compiler.inserting_chars)
    {
        *compiler.out << '"';

        // FIXME: Generate correct results for special characters (e.g. tabs should become '\t')
        for (size_t i = 0; i < values.size(); i++)
            *compiler.out << (char)values.at(i);

        *compiler.out << '"';
    }
    else if (compiler.insert_stmt)
    {
        for (size_t i = 0; i < values.size(); i++)
        {
            if (i > 0)
                *compiler.out << "<<";
            *compiler.out << values.at(i);
        }
    }
    else
    {
        *compiler.out << '{';

        for (size_t i = 0; i < values.size(); i++)
        {
            if (i > 0)
                *compiler.out << ',';
            *compiler.out << values.at(i);
        }

        *compiler.out << '}';
    }
}

void compile_expression(Compiler &compiler, Scope &scope)
{
    if (peek_call(compiler))
        compile_call(compiler, scope);
    else if (peek(compiler) == TokenKind::String)
        compile_list_literal(compiler);
    else if (peek(compiler) == TokenKind::Identity)
        compile_identity(compiler, scope);
    else
        error(compiler, "Expected expression");
}

bool peek_token_in_statement(const Compiler &compiler, TokenKind kind)
{
    TokenKind t;
    size_t i = 0;
    do
    {
        t = peek_ahead(compiler, i);
        if (t == TokenKind::Line || t == TokenKind::EndOfFile)
            return false;
        i++;
    } while (t != kind);
    return true;
}

bool peek_ltr_insert_stmt(const Compiler &compiler)
{
    return peek_token_in_statement(compiler, TokenKind::InsertR);
}

bool peek_rtl_insert_stmt(const Compiler &compiler)
{
    return peek_token_in_statement(compiler, TokenKind::InsertL);
}

void compile_ltr_insert_stmt(Compiler &compiler, Scope &scope)
{
    compiler.insert_stmt = true;
    compiler.inserting_ltr = true;
    compiler.inserting_chars = false;
    compile_expression(compiler, scope);

    eat(compiler, TokenKind::InsertR, "Expected >> operator.");
    *compiler.out << ">>";

    compile_expression(compiler, scope);

    while (match(compiler, TokenKind::InsertR))
    {
        *compiler.out << ">>";
        compile_expression(compiler, scope);
    }

    compiler.insert_stmt = false;
}

void compile_rtl_insert_stmt(Compiler &compiler, Scope &scope)
{
    compiler.insert_stmt = true;
    compiler.inserting_ltr = false;
    compiler.inserting_chars = false;
    compile_expression(compiler, scope);

    eat(compiler, TokenKind::InsertL, "Expected << operator.");
    *compiler.out << "<<";

    compile_expression(compiler, scope);

    while (match(compiler, TokenKind::InsertL))
    {
        *compiler.out << "<<";
        compile_expression(compiler, scope);
    }

    compiler.insert_stmt = false;
}

bool peek_statement(const Compiler &compiler)
{
    return peek_expression(compiler);
}

void compile_statement(Compiler &compiler, Scope &scope)
{
    skip_lines(compiler);

    stringstream *parent_stream = compiler.out;
    stringstream statement;
    compiler.out = &statement;

    if (peek_ltr_insert_stmt(compiler))
        compile_ltr_insert_stmt(compiler, scope);
    else if (peek_rtl_insert_stmt(compiler))
        compile_rtl_insert_stmt(compiler, scope);
    else
        compile_expression(compiler, scope);

    eat(compiler, TokenKind::Line, "Expected newline to terminate statement");

    compiler.out = parent_stream;
    *compiler.out << statement.rdbuf();
}

void compile_statement_block(Compiler &compiler, Scope &scope)
{
    // FIXME: Give line numbers when eating '{' or '}' fails.
    eat(compiler, TokenKind::CurlyL, "Expected '{' to open block.");
    *compiler.out << '{';

    skip_lines(compiler);

    Scope block_scope = Scope(&scope);

    compiler.out_statement_block = compiler.out;
    while (peek_statement(compiler))
    {
        compile_statement(compiler, block_scope);
        *compiler.out << ";";
    }

    if (compiler.in_main)
        *compiler.out << "return 0;";
    compiler.out_statement_block = nullptr;

    eat(compiler, TokenKind::CurlyR, "Expected '}' to close block.");
    *compiler.out << '}';
}

void compile_function(Compiler &compiler, Scope &scope)
{
    string identity = eat(compiler, TokenKind::Identity, "Expected function name.").str;
    compiler.in_main = identity == "main";

    declare(scope, identity, DeclarableKind::Function);

    *compiler.out
        << (compiler.in_main ? "int " : "void ")
        << identity
        << "(";

    eat(compiler, TokenKind::ParenL, "Expected '(' after function name.");
    // TODO: Parse and generate function parameters
    eat(compiler, TokenKind::ParenR, "Expected ')' at end of function parameters.");

    *compiler.out << ")";
    compile_statement_block(compiler, scope);
    compiler.in_main = false;
}

void compile_program(Compiler &compiler)
{
    stringstream program;
    compiler.out = &program;

    Scope program_scope;

    skip_lines(compiler);
    while (peek(compiler) == TokenKind::Identity)
    {
        compile_function(compiler, program_scope);
        skip_lines(compiler);
    }
    eat(compiler, TokenKind::EndOfFile, "Expected end of file");

    string out_path = "local/output.cpp";
    std::ofstream out_file;
    out_file.open(out_path, std::ios::out);
    if (!out_file)
    {
        error(compiler, "Output file " + out_path + " could not be loaded");
        return;
    }
    out_file << "#include <iostream>\n";
    out_file << program.rdbuf();
    out_file.close();
}

// MAIN //

int main(int argc, char *argv[])
{
    string src_path = (argc == 2) ? ("../" + (string)argv[1]) : "../samples/compiler-test.tiny";

    std::ifstream src_file;
    src_file.open(src_path, std::ios::in);
    if (!src_file)
    {
        cout << "Source file " << src_path << " could not be loaded" << endl;
        return 0;
    }
    std::string src((std::istreambuf_iterator<char>(src_file)), std::istreambuf_iterator<char>());
    src_file.close();

    Lexer lexer(&src);
    while (!lexer.finished)
        next_token(lexer);

    Compiler compiler(&lexer);
    compile_program(compiler);

    cout << "FINISH" << endl;

    return 0;
}