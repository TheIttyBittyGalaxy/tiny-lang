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

// VECTOR UTIL //

template <typename T>
struct ptr
{
private:
    vector<T> *container = nullptr;
    size_t index = 0;

public:
    ptr() : container(nullptr), index(0){};
    ptr(vector<T> *container, size_t index) : container(container), index(index){};
    T *operator->() { return container != nullptr ? (&this->container->at(index)) : nullptr; }
};

template <typename T>
using list = vector<ptr<T>>;

template <typename T>
ptr<T> yoink(vector<T> *vec)
{
    size_t index = vec->size();
    vec->emplace_back();
    return ptr<T>(vec, index);
}

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

    STRING,

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

// LEXING //

struct Lexer
{
    string *src;
    size_t token_pos;
    size_t current_pos;
    int current_line;
    Token current;
    Token next;

    Lexer(string *src) : src(src),
                         token_pos(0),
                         current_pos(0),
                         current_line(1) {}
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
    lexer.next.kind = kind;
    size_t length = (lexer.current_pos - lexer.token_pos);
    lexer.next.str = lexer.src->substr(lexer.token_pos, length);
    lexer.next.line = kind != TokenKind::Line ? lexer.current_line : lexer.current_line - 1;
}

void next_token(Lexer &lexer)
{
    lexer.current = lexer.next;

    cout
        << std::setfill('0') << std::setw(2) << (int)lexer.current.kind << " "
        << ((lexer.current.kind == TokenKind::Line)
                ? "new line"
                : lexer.current.str)
        << endl;

    if (lexer.next.kind == TokenKind::EndOfFile || lexer.current.kind == TokenKind::EndOfFile)
        return;

    while (peek(lexer) != '\0')
    {
        while (peek(lexer) == ' ' || peek(lexer) == '\t')
            next(lexer);

        lexer.token_pos = lexer.current_pos;

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
            make_token(lexer, TokenKind::STRING);
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

    lexer.token_pos = lexer.current_pos;
    make_token(lexer, TokenKind::EndOfFile);
}

// PROGRAM MODEL //

// Expressions

struct UnresolvedId
{
    Token identity;
};

struct ValueList
{
    vector<int> values;
};

enum class ExprKind
{
    Null,
    UnresolvedId,
    ValueList,
};

struct Expression
{
    ExprKind kind;
    union
    {
        ptr<UnresolvedId> unresolved_id;
        ptr<ValueList> value_list;
    };
    Expression() : kind(ExprKind::Null), unresolved_id() {}
};

void set_expression(ptr<Expression> expr, ptr<UnresolvedId> unresolved_id)
{
    expr->unresolved_id = unresolved_id;
    expr->kind = ExprKind::UnresolvedId;
}

void set_expression(ptr<Expression> expr, ptr<ValueList> value_list)
{
    expr->value_list = value_list;
    expr->kind = ExprKind::ValueList;
}

// Statements

struct StmtInsert
{
    ptr<Expression> subject;
    ptr<Expression> insert;
    bool insert_at_end;
};

enum class StmtKind
{
    Null,
    Expression,
    Insert,
};

struct Statement
{
    StmtKind kind;
    union
    {
        ptr<Expression> expr;
        ptr<StmtInsert> insert;
    };
    Statement() : kind(StmtKind::Null), expr() {}
};

void set_statement(ptr<Statement> stmt, ptr<Expression> expr)
{
    stmt->expr = expr;
    stmt->kind = StmtKind::Expression;
}

void set_statement(ptr<Statement> stmt, ptr<StmtInsert> insert)
{
    stmt->insert = insert;
    stmt->kind = StmtKind::Insert;
}

// Program

struct Scope
{
    ptr<Scope> parent;
    list<Statement> statements;
};

struct Function
{
    Token identity;
    ptr<Scope> scope;
};

struct Program
{
    vector<UnresolvedId> unresolved_ids;
    vector<ValueList> value_lists;
    vector<Expression> expressions;
    vector<StmtInsert> stmt_inserts;
    vector<Statement> statements;
    vector<Scope> scopes;
    vector<Function> functions;
};

// PARSING //

struct Parser
{
    Lexer *lexer;
    Program *program;

    Parser(Lexer *lexer, Program *program) : lexer(lexer), program{program} {}
};

void error(const Parser &parser, const string msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    Token current = parser.lexer->current;
    cout << "Error on line " << current.line << ": " << msg;
    if (current.kind == TokenKind::Line)
        cout << " (got new line)";
    else
        cout << " (got " << std::setfill('0') << std::setw(2) << (int)current.kind << " " << current.str << ")";
    cout << endl;
}

TokenKind peek(const Parser &parser)
{
    return parser.lexer->current.kind;
}

TokenKind peek_next(const Parser &parser)
{
    return parser.lexer->next.kind;
}

bool match(Parser &parser, TokenKind kind)
{
    if (peek(parser) != kind)
        return false;
    next_token(*parser.lexer);
    return true;
}

Token eat(Parser &parser, TokenKind kind, const string msg)
{
    if (kind != TokenKind::Line)
    {
        while (match(parser, TokenKind::Line))
            ;
    }

    Token t = parser.lexer->current;
    if (!match(parser, kind))
        error(parser, msg);
    return t;
}

bool peek_expression(const Parser &parser)
{
    return peek(parser) == TokenKind::Identity ||
           peek(parser) == TokenKind::STRING;
}

ptr<Expression> parse_expression(Parser &parser)
{
    ptr<Expression> expr = yoink(&parser.program->expressions);

    if (peek(parser) == TokenKind::Identity)
    {
        ptr<UnresolvedId> unresolved_id = yoink(&parser.program->unresolved_ids);
        unresolved_id->identity = eat(parser, TokenKind::Identity, "Expected identity.");
        set_expression(expr, unresolved_id);
    }
    else if (peek(parser) == TokenKind::STRING)
    {
        ptr<ValueList> value_list = yoink(&parser.program->value_lists);

        Token token = eat(parser, TokenKind::STRING, "Expected string.");
        for (int i = 1; i <= token.str.length() - 2; i++)
            value_list->values.push_back((int)token.str[i]);

        set_expression(expr, value_list);
    }
    else
    {
        error(parser, "Expected expression");
    }

    return expr;
}

bool peek_statement(const Parser &parser)
{
    return peek_expression(parser);
}

void parse_statement(Parser &parser, ptr<Scope> scope)
{
    ptr<Statement> stmt = yoink(&parser.program->statements);
    ptr<Expression> expr = parse_expression(parser);

    if (peek(parser) == TokenKind::InsertL || peek(parser) == TokenKind::CurlyR)
    {
        ptr<StmtInsert> insert = yoink(&parser.program->stmt_inserts);
        insert->subject = expr;

        if (match(parser, TokenKind::InsertL))
            insert->insert_at_end = true;
        else if (match(parser, TokenKind::InsertR))
            insert->insert_at_end = false;
        else
            error(parser, "Expected insertion operator.");

        insert->insert = parse_expression(parser);
        set_statement(stmt, insert);
    }
    else
    {
        set_statement(stmt, expr);
    }

    scope->statements.push_back(stmt);
}

void parse_scope(Parser &parser, ptr<Scope> scope, ptr<Scope> parent)
{
    scope->parent = parent;

    bool statement_block = match(parser, TokenKind::CurlyL);

    if (statement_block)
    {
        while (match(parser, TokenKind::Line))
            ;

        while (peek_statement(parser))
        {
            parse_statement(parser, scope);
            eat(parser, TokenKind::Line, "Expected newline to terminate statement");
            while (match(parser, TokenKind::Line))
                ;
        }

        // FIXME: Give the line number of the '{' we are trying to close
        eat(parser, TokenKind::CurlyR, "Expected '}' to close block.");
    }
    else if (peek_statement(parser))
    {
        parse_statement(parser, scope);
    }
    else
    {
        error(parser, "Expected '{' to begin statement block.");
    }
}

void parse_function(Parser &parser, Token identity)
{
    ptr<Function> funct = yoink(&parser.program->functions);
    funct->scope = yoink(&parser.program->scopes);
    funct->identity = identity;

    eat(parser, TokenKind::ParenL, "Expected '(' after function name.");
    // FIXME: Parse function parameters
    eat(parser, TokenKind::ParenR, "Expected ')' at end of function arguments.");

    parse_scope(parser, funct->scope, ptr<Scope>());
}

void parse_program(Parser &parser)
{
    while (peek(parser) == TokenKind::Identity)
    {
        Token identity = eat(parser, TokenKind::Identity, "Expected function or variable declaration.");
        if (peek(parser) == TokenKind::ParenL)
        {
            parse_function(parser, identity);
            continue;
        }
        error(parser, "Expected function or variable declaration.");
    }

    eat(parser, TokenKind::EndOfFile, "Expected end of file");
}

// GENERATE //

struct Generator
{
    Program *program;
    std::ofstream f;

    bool insert_stmt = false;
    bool inserting_chars = false;

    Generator(Program *program) : program(program){};
};

// FIXME: We shouldn't actually be trying to generate unresolved ids. This is just a hack.
void generate(Generator &generator, ptr<UnresolvedId> unresolved_id)
{
    if (unresolved_id->identity.str == "console")
    {
        generator.f << "std::cout";
        if (generator.insert_stmt)
            generator.inserting_chars = true;
    }
}

void generate(Generator &generator, ptr<ValueList> value_list)
{
    if (generator.inserting_chars)
    {
        generator.f << '"';

        // FIXME: Generate correct results for special characters (e.g. tabs should become '\t')
        for (size_t i = 0; i < value_list->values.size(); i++)
            generator.f << (char)value_list->values.at(i);

        generator.f << '"';
    }
    else if (generator.insert_stmt)
    {
        for (size_t i = 0; i < value_list->values.size(); i++)
        {
            if (i > 0)
                generator.f << "<<";
            generator.f << value_list->values.at(i);
        }
    }
    else
    {
        generator.f << '{';

        for (size_t i = 0; i < value_list->values.size(); i++)
        {
            if (i > 0)
                generator.f << ',';
            generator.f << value_list->values.at(i);
        }

        generator.f << '}';
    }
}

void generate(Generator &generator, ptr<Expression> expr)
{
    switch (expr->kind)
    {
    case ExprKind::UnresolvedId:
        // FIXME: We shouldn't actually be trying to generate unresolved ids. This is just a hack.
        generate(generator, expr->unresolved_id);
        break;

    case ExprKind::ValueList:
        generate(generator, expr->value_list);
        break;

    default:
        cout << "Error. Unable to generate statement";
        error_has_occoured = true;
        break;
    }
}

void generate(Generator &generator, ptr<StmtInsert> insert)
{
    generator.insert_stmt = true;
    generator.inserting_chars = false;

    // FIXME: Generate correct code when we are inserting to the front of the subject
    generate(generator, insert->subject);
    generator.f << "<<";
    generate(generator, insert->insert);

    generator.insert_stmt = false;
}

void generate(Generator &generator, ptr<Statement> stmt)
{
    switch (stmt->kind)
    {
    case StmtKind::Expression:
        generate(generator, stmt->expr);
        break;

    case StmtKind::Insert:
        generate(generator, stmt->insert);
        break;

    default:
        cout << "Error. Unable to generate statement";
        error_has_occoured = true;
        break;
    }
}

void generate(Generator &generator, ptr<Scope> scope)
{
    for (size_t i = 0; i < scope->statements.size(); i++)
    {
        generate(generator, scope->statements.at(i));
        generator.f << ";";
    }
}

void generate(Generator &generator, ptr<Function> funct)
{
    generator.f
        << (funct->identity.str == "main" ? "int " : "void ")
        << funct->identity.str
        << "(";

    // TODO: Generate function arguments
    generator.f << "){";

    generate(generator, funct->scope);

    if (funct->identity.str == "main")
        generator.f << "return 0;";

    generator.f << "}";
}

void generate_program(Generator &generator)
{
    generator.f.open("local/output.cpp", std::ios::out);

    generator.f << "#include <iostream>\n";

    for (size_t i = 0; i < generator.program->functions.size(); i++)
        generate(generator, ptr<Function>(&generator.program->functions, i));

    generator.f.close();
}

// COMPILE //

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

    Program program;
    Lexer lexer(&src);
    Parser parser(&lexer, &program);
    Generator generator(&program);

    next_token(lexer);
    next_token(lexer);
    parse_program(parser);
    generate_program(generator);

    cout << "FINISH" << endl;

    return 0;
}