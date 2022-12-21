#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string.h>
#include <string>
#include <vector>
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

    Comma,
    Assign,
    Line,

    ParenL,
    ParenR,
    CurlyL,
    CurlyR,
    SquareL,
    SquareR,

    InsertL,
    InsertR,

    LessThan,
    LessThanEqual,
    GreaterThan,
    GreaterThanEqual,

    Return,

    String,
    Identity,

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
    vector<Token> tokens;

    bool finished = false;
    size_t position = 0;
    size_t line = 0;
    size_t token_position = 0;

    Lexer(string *src) : src(src) {}
};

void error(const Lexer &lexer, const string msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    cout << "Error on line " << lexer.line << ": " << msg << endl;
}

char peek(const Lexer &lexer)
{
    return (*lexer.src)[lexer.position];
}

char next(Lexer &lexer)
{
    char c = peek(lexer);
    lexer.position++;
    if (c == '\n')
        lexer.line++;
    return c;
}

bool match(Lexer &lexer, const char c)
{
    if (peek(lexer) != c)
        return false;
    next(lexer);
    return true;
}

// Assumes that the first character of the word has already been matched
bool match_word(Lexer &lexer, string word)
{
    size_t len = word.length() - 1;
    bool match = lexer.src->substr(lexer.position, len) == word.substr(1, len);
    if (match)
        lexer.position += len;
    return match;
}

bool char_is_name(const char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void make_token(Lexer &lexer, const TokenKind kind)
{
    Token t;
    t.kind = kind;
    size_t length = (lexer.position - lexer.token_position);
    t.str = lexer.src->substr(lexer.token_position, length);
    t.line = kind != TokenKind::Line ? lexer.line : lexer.line - 1;

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

    lexer.token_position = lexer.position;

    if (peek(lexer) == '\0')
    {
        make_token(lexer, TokenKind::EndOfFile);
        lexer.finished = true;
        return;
    }

    char c = next(lexer);
    switch (c)
    {

    case ',':
        make_token(lexer, TokenKind::Comma);
        return;
    case '=':
        make_token(lexer, TokenKind::Assign);
        return;
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
    case '[':
        make_token(lexer, TokenKind::SquareL);
        return;
    case ']':
        make_token(lexer, TokenKind::SquareR);
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
        if (match_word(lexer, "return"))
        {
            make_token(lexer, TokenKind::Return);
            return;
        }

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

enum class TinyType
{
    Unspecified,
    Value,
    List,
    Console,
    None,
};

enum class EntityKind
{
    Null,
    Variable,
    Function,
};

// TODO: Could 'kind' and 'type' potentially be collapsed all into one thing?
struct Entity
{
    EntityKind kind = EntityKind::Null;
    string identity = "";
    string c_identity = "_ERROR_NULL_ENTITY";
    union
    {
        struct
        {
            TinyType type;
        } variable;
        struct
        {
            TinyType return_type;
        } function;
    };
};

struct Scope
{
    std::map<string, Entity> entities;
    Scope *parent = nullptr;

    Scope(){};
    Scope(Scope *parent) : parent(parent){};
};

Entity *fetch(Scope *scope, string id)
{
    auto got = scope->entities.find(id);
    if (got != scope->entities.end())
        return &got->second;

    if (scope->parent != nullptr)
        return fetch(scope->parent, id);

    return nullptr;
}

Entity *declare(Scope &scope, string id, EntityKind kind)
{
    Entity dec;

    dec.identity = id;
    // FIXME: Find a proper policy for generating the C++ identifiers that won't clash
    dec.c_identity = id == "main" ? id : id + "_";
    dec.kind = kind;

    if (kind == EntityKind::Variable)
    {
        dec.variable.type = TinyType::Unspecified;
    }
    else if (kind == EntityKind::Function)
    {
        dec.function.return_type = TinyType::Unspecified;
    }

    scope.entities[id] = dec;
    return &scope.entities[id];
}

// COMPILER //

struct Compiler
{
    Lexer *lexer;
    size_t current_token = 0;

    stringstream *out = nullptr;

    bool insert_stmt = false;
    bool inserting_chars = false;
    bool inserting_ltr = false;
    bool in_main = false;
    Entity *in_function = nullptr;
    TinyType function_returns;

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

TinyType compile_expression(Compiler &compiler, Scope &scope);

TinyType compile_identity(Compiler &compiler, Scope &scope)
{
    Token identity = eat(compiler, TokenKind::Identity, "Expected identity.");
    // FIXME: Have a console variable declared in the global scope
    if (identity.str == "console")
    {
        *compiler.out << (compiler.inserting_ltr ? "std::cin" : "std::cout");

        // FIXME: This should only occour if it happens in the first expression of the insert statement
        if (!compiler.inserting_ltr)
            compiler.inserting_chars = true;

        return TinyType::Console;
    }
    else
    {
        string id = identity.str;
        Entity *dec = fetch(&scope, id);
        bool valid_variable = true;

        if (dec == nullptr)
        {
            dec = declare(scope, id, EntityKind::Variable);
        }
        else if (dec->kind == EntityKind::Function)
        {
            valid_variable = false;
            error(compiler, "Cannot use function as an expression.");
        }

        *compiler.out << dec->c_identity;

        return valid_variable ? dec->variable.type : TinyType::Unspecified;
    }

    return TinyType::Unspecified;
}

TinyType compile_call(Compiler &compiler, Scope &scope)
{
    string id = eat(compiler, TokenKind::Identity, "Expected function name.").str;
    Entity *funct = fetch(&scope, id);
    bool valid_function = true;

    if (funct == nullptr)
    {
        valid_function = false;
        error(compiler, "Function '" + id + "' does not exist.");
    }
    else if (funct->kind != EntityKind::Function)
    {
        valid_function = false;
        error(compiler, "'" + id + "' is not a function.");
    }

    eat(compiler, TokenKind::ParenL, "Expected '(' after function name.");
    *compiler.out
        << (valid_function ? funct->c_identity : id)
        << "(";

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

    return valid_function ? funct->function.return_type : TinyType::Unspecified;
}

TinyType compile_list_literal(Compiler &compiler)
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

    return TinyType::List;
}

TinyType compile_expression(Compiler &compiler, Scope &scope)
{
    if (peek_call(compiler))
        return compile_call(compiler, scope);
    else if (peek(compiler) == TokenKind::String)
        return compile_list_literal(compiler);
    else if (peek(compiler) == TokenKind::Identity)
        return compile_identity(compiler, scope);
    else
        error(compiler, "Expected expression");

    return TinyType::Unspecified;
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

bool peek_assign_stmt(const Compiler &compiler)
{
    return peek(compiler) == TokenKind::Identity && (peek_ahead(compiler, 1) == TokenKind::SquareL ||
                                                     peek_ahead(compiler, 1) == TokenKind::Assign);
}

bool peek_ltr_insert_stmt(const Compiler &compiler)
{
    return peek_token_in_statement(compiler, TokenKind::InsertR);
}

bool peek_rtl_insert_stmt(const Compiler &compiler)
{
    return peek_token_in_statement(compiler, TokenKind::InsertL);
}

bool peek_return_stmt(const Compiler &compiler)
{
    return peek(compiler) == TokenKind::Return;
}

bool peek_statement(const Compiler &compiler)
{
    return peek_expression(compiler) ||
           peek_return_stmt(compiler);
}

void compile_assign_stmt(Compiler &compiler, Scope &scope)
{
    string id = eat(compiler, TokenKind::Identity, "Expected variable name.").str;
    Entity *dec = fetch(&scope, id);
    bool already_existed = dec != nullptr;

    if (dec == nullptr)
        dec = declare(scope, id, EntityKind::Variable);

    if (dec->kind != EntityKind::Variable)
    {
        error(compiler, "Cannot assign to '" + id + "' as it is not a variable.");
        return;
    }

    if (match(compiler, TokenKind::SquareL))
    {
        if (!already_existed)
            dec->variable.type = TinyType::List;
        else if (dec->variable.type != TinyType::List)
            error(compiler, "Variable '" + id + "' cannot be redeclared as a list.");
        eat(compiler, TokenKind::SquareR, "Expected ']'");
    }

    if (match(compiler, TokenKind::Assign))
    {
        // FIXME: Implement variable assignment
        error(compiler, "Variable assignment not yet implemented.");
    }

    if (dec->variable.type == TinyType::Unspecified)
        dec->variable.type = TinyType::Value;

    *compiler.out << dec->c_identity;
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

void compile_return_stmt(Compiler &compiler, Scope &scope)
{
    eat(compiler, TokenKind::Return, "Expected 'return'");
    *compiler.out << "return";
    TinyType return_type = TinyType::None;
    if (peek_expression(compiler))
    {
        *compiler.out << " ";
        return_type = compile_expression(compiler, scope);
    }

    if (compiler.function_returns == TinyType::Unspecified)
        compiler.function_returns = return_type;
    else if (compiler.function_returns != return_type)
        error(compiler, "Incorrect return type");
}

void compile_statement(Compiler &compiler, Scope &scope)
{
    skip_lines(compiler);

    stringstream *parent_stream = compiler.out;
    stringstream statement;
    compiler.out = &statement;

    if (peek_assign_stmt(compiler))
        compile_assign_stmt(compiler, scope);
    else if (peek_ltr_insert_stmt(compiler))
        compile_ltr_insert_stmt(compiler, scope);
    else if (peek_rtl_insert_stmt(compiler))
        compile_rtl_insert_stmt(compiler, scope);
    else if (peek_return_stmt(compiler))
        compile_return_stmt(compiler, scope);
    else
        compile_expression(compiler, scope);

    eat(compiler, TokenKind::Line, "Expected newline to terminate statement");

    compiler.out = parent_stream;
    *compiler.out << statement.rdbuf();
}

void compile_statement_block(Compiler &compiler, Scope &scope)
{
    eat(compiler, TokenKind::CurlyL, "Expected '{' to open block.");
    *compiler.out << '{';

    skip_lines(compiler);

    Scope block_scope = Scope(&scope);

    stringstream *parent_stream = compiler.out;
    stringstream block_body;
    stringstream block_declarations;
    compiler.out = &block_body;

    while (peek_statement(compiler))
    {
        compile_statement(compiler, block_scope);
        *compiler.out << ";";
    }

    if (compiler.in_main)
        *compiler.out << "return 0;";

    compiler.out = parent_stream;

    for (auto &it : block_scope.entities)
    {
        Entity *var = &it.second;
        if (var->kind == EntityKind::Variable)
        {
            *compiler.out
                << (var->variable.type == TinyType::Value ? "value " : "list ")
                << var->c_identity
                << ";";
        }
    }

    *compiler.out << block_body.rdbuf();

    eat(compiler, TokenKind::CurlyR, "Expected '}' to close block.");
    *compiler.out << '}';
}

void compile_parameter(Compiler &compiler, Scope &scope)
{
    string id = eat(compiler, TokenKind::Identity, "Expected parameter name.").str;
    Entity *param = declare(scope, id, EntityKind::Variable);
    param->variable.type = TinyType::Value;

    if (match(compiler, TokenKind::SquareL))
    {
        eat(compiler, TokenKind::SquareR, "Expected ']'");
        param->variable.type = TinyType::List;
    }

    *compiler.out
        << (param->variable.type == TinyType::Value ? "value " : "list ")
        << param->c_identity;
}

void compile_function(Compiler &compiler, Scope &scope)
{
    stringstream *parent_stream = compiler.out;
    stringstream function_body;
    compiler.out = &function_body;

    Scope function_scope;

    string identity = eat(compiler, TokenKind::Identity, "Expected function name.").str;
    compiler.in_main = identity == "main";
    compiler.function_returns = TinyType::Unspecified;

    Entity *fun = declare(scope, identity, EntityKind::Function);
    compiler.in_function = fun;

    *compiler.out << fun->c_identity << "(";

    eat(compiler, TokenKind::ParenL, "Expected '(' after function name.");
    if (peek(compiler) == TokenKind::Identity)
    {
        compile_parameter(compiler, function_scope);
        while (match(compiler, TokenKind::Comma))
        {
            *compiler.out << ",";
            compile_parameter(compiler, function_scope);
        }
    }
    eat(compiler, TokenKind::ParenR, "Expected ')' at end of function parameters.");

    *compiler.out << ")";
    compile_statement_block(compiler, function_scope);

    compiler.out = parent_stream;

    if (compiler.function_returns == TinyType::Unspecified)
        compiler.function_returns = TinyType::None;

    if (compiler.in_main)
    {
        // FIXME: Determine what the correct behaviour here should actually be.
        *compiler.out << "int ";
    }
    else
    {
        switch (compiler.function_returns)
        {
        case TinyType::None:
            *compiler.out << "void ";
            break;
        case TinyType::Value:
            *compiler.out << "value ";
            break;
        case TinyType::List:
            *compiler.out << "list ";
            break;
        default:
            error(compiler, "Unable to generate C++ function return type.");
            break;
        }
    }
    *compiler.out << function_body.rdbuf();
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
    out_file
        << "#include <iostream>\n"
        << "#include <vector>\n"
        << "using value = int;\n"
        << "using list = std::vector<int>;\n"
        << program.rdbuf();
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