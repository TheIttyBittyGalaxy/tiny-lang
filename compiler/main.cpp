#include <string.h>
#include <string>
#include <iomanip>
#include <iostream>
#include <vector>
#include <fstream>
using std::cout;
using std::endl;
using std::string;
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

// COMPILER //

struct Compiler
{
    Lexer *lexer;
    std::ofstream out;
    size_t current_token = 0;
    bool insert_stmt = false;
    bool inserting_chars = false;
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

Token eat(Compiler &compiler, TokenKind kind, const string msg)
{
    if (kind != TokenKind::Line)
    {
        while (match(compiler, TokenKind::Line))
            ;
    }

    Token t = current_token(compiler);
    if (!match(compiler, kind))
        error(compiler, msg);
    return t;
}

void compile_identity(Compiler &compiler)
{
    Token identity = eat(compiler, TokenKind::Identity, "Expected identity.");
    if (identity.str == "console")
    {
        compiler.out << "std::cout";

        // FIXME: This should only occour if it happens in the first expression of the insert statement
        if (compiler.insert_stmt)
            compiler.inserting_chars = true;
    }
    else
    {
        cout << identity.str;
    }
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
        compiler.out << '"';

        // FIXME: Generate correct results for special characters (e.g. tabs should become '\t')
        for (size_t i = 0; i < values.size(); i++)
            compiler.out << (char)values.at(i);

        compiler.out << '"';
    }
    else if (compiler.insert_stmt)
    {
        for (size_t i = 0; i < values.size(); i++)
        {
            if (i > 0)
                compiler.out << "<<";
            compiler.out << values.at(i);
        }
    }
    else
    {
        compiler.out << '{';

        for (size_t i = 0; i < values.size(); i++)
        {
            if (i > 0)
                compiler.out << ',';
            compiler.out << values.at(i);
        }

        compiler.out << '}';
    }
}

bool peek_expression(const Compiler &compiler)
{
    return peek(compiler) == TokenKind::Identity ||
           peek(compiler) == TokenKind::String;
}

void compile_expression(Compiler &compiler)
{
    switch (peek(compiler))
    {
    case TokenKind::Identity:
        compile_identity(compiler);
        break;

    case TokenKind::String:
        compile_list_literal(compiler);
        break;

    default:
        error(compiler, "Expected expression");
        break;
    }
}

bool peek_insert_stmt(const Compiler &compiler)
{
    TokenKind t;
    size_t i = 0;
    do
    {
        t = peek_ahead(compiler, i);

        if (t == TokenKind::InsertL || t == TokenKind::InsertR)
            return true;

        if (t == TokenKind::Line)
            return false;

        i++;
    } while (t != TokenKind::EndOfFile);
    return false;
}

void compile_insert_stmt(Compiler &compiler)
{
    compiler.insert_stmt = true;
    compiler.inserting_chars = false;
    compile_expression(compiler);

    eat(compiler, TokenKind::InsertL, "Expected << operator.");
    compiler.out << "<<";

    compile_expression(compiler);

    while (match(compiler, TokenKind::InsertL))
    {
        compiler.out << "<<";
        compile_expression(compiler);
    }

    compiler.insert_stmt = false;
}

bool peek_statement(const Compiler &compiler)
{
    return peek_expression(compiler);
}

void compile_statement(Compiler &compiler)
{
    while (match(compiler, TokenKind::Line))
        ;

    if (peek_insert_stmt(compiler))
        compile_insert_stmt(compiler);
    else
        compile_expression(compiler);

    eat(compiler, TokenKind::Line, "Expected newline to terminate statement");
}

void compile_statement_block(Compiler &compiler)
{
    // FIXME: Give line numbers when eating '{' or '}' fails.
    eat(compiler, TokenKind::CurlyL, "Expected '{' to open block.");
    compiler.out << '{';

    while (match(compiler, TokenKind::Line))
        ;

    while (peek_statement(compiler))
    {
        compile_statement(compiler);
        compiler.out << ";";
    }

    if (compiler.in_main)
        compiler.out << "return 0;";

    eat(compiler, TokenKind::CurlyR, "Expected '}' to close block.");
    compiler.out << '}';
}

void compile_function(Compiler &compiler)
{
    Token identity = eat(compiler, TokenKind::Identity, "Expected function name.");
    compiler.in_main = identity.str == "main";

    compiler.out
        << (compiler.in_main ? "int " : "void ")
        << identity.str
        << "(";

    eat(compiler, TokenKind::ParenL, "Expected '(' after function name.");
    // TODO: Parse and generate function parameters
    eat(compiler, TokenKind::ParenR, "Expected ')' at end of function parameters.");

    compiler.out << ")";
    compile_statement_block(compiler);
    compiler.in_main = false;
}

void compile_program(Compiler &compiler)
{
    string out_path = "local/output.cpp";
    compiler.out.open(out_path, std::ios::out);
    if (!compiler.out)
    {
        error(compiler, "Source file " + out_path + " could not be loaded");
        return;
    }

    compiler.out << "#include <iostream>\n";
    while (peek(compiler) == TokenKind::Identity)
        compile_function(compiler);

    eat(compiler, TokenKind::EndOfFile, "Expected end of file");
    compiler.out.close();
}

// MAIN //

int main(int argc, char *argv[])
{
    string src_path = (argc == 2) ? ("../" + (string)argv[1]) : "../samples/hello.tiny";

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