#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cstring>

// ARRAYS //

template <typename T>
struct Array
{
    T *value = nullptr;
    size_t count = 0;
    size_t capacity = 0;
};

template <typename T>
void clear(Array<T> &array)
{
    if (array.value == nullptr)
        return;
    free(array.value);
    array.value = nullptr;
    array.count = 0;
    array.capacity = 0;
}

template <typename T>
void allocate_enough_room(Array<T> &array)
{
    if (array.count < array.capacity)
        return;
    array.capacity = array.capacity == 0 ? 8 : array.capacity * 1.5;
    array.value = (T *)realloc(array.value, array.capacity);
}

template <typename T>
void append(Array<T> &array, T value)
{
    allocate_enough_room(array);
    array.value[array.count] = value;
    array.count++;
}

template <typename T>
T *yoink(Array<T> &array)
{
    allocate_enough_room(array);
    array.count++;
    return &array.value[array.count - 1];
}

// STRING //

struct str
{
    const char *first;
    int length;
};

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
    str string;
    int line;

    Token() : kind(TokenKind::Null),
              string({nullptr, 0}),
              line(0) {}

    Token(TokenKind kind, str string, int line) : kind(kind),
                                                  string(string),
                                                  line(line) {}
};

// LEXING //

struct Lexer
{
    const char *src;
    const char *token_first;
    const char *current_char;
    int current_line;
    Token current;
    Token next;

    Lexer(const char *src) : src(src),
                             token_first(src),
                             current_char(src),
                             current_line(1) {}
};

void error(const Lexer &lexer, const char *msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    printf("Error on line %d: %s\n", lexer.current_line, msg);
}

char peek(const Lexer &lexer)
{
    return *lexer.current_char;
}

char peek(const Lexer &lexer, const int ahead)
{
    return peek(lexer) == '\0' ? '\0' : *(lexer.current_char + ahead);
}

char next(Lexer &lexer)
{
    char c = peek(lexer);
    lexer.current_char++;
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
    lexer.next.string.first = lexer.token_first;
    lexer.next.string.length = (int)(lexer.current_char - lexer.token_first);
    lexer.next.line = kind != TokenKind::Line ? lexer.current_line : lexer.current_line - 1;
}

void next_token(Lexer &lexer)
{
    lexer.current = lexer.next;

    char value[] = "";
    if (lexer.current.kind == TokenKind::Line)
        strcat(value, "new line");
    else
        strncat(value, lexer.current.string.first, lexer.current.string.length);
    printf("%02d %s\n", lexer.current.kind, value);

    if (lexer.next.kind == TokenKind::EndOfFile || lexer.current.kind == TokenKind::EndOfFile)
        return;

    while (peek(lexer) != '\0')
    {
        while (peek(lexer) == ' ' || peek(lexer) == '\t')
            next(lexer);

        lexer.token_first = lexer.current_char;

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

            char msg[] = "Unable to parse character '";
            strncat(msg, &c, 1);
            strcat(msg, "'");
            error(lexer, msg);
            return;
        }
        }
    }

    lexer.token_first = lexer.current_char;
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
    Array<int> values;
};

enum class ExprKind
{
    UnresolvedId,
    ValueList,
};

struct Expression
{
    ExprKind kind;
    union
    {
        UnresolvedId *unresolved_id;
        ValueList *value_list;
    };
};

void set_expression(Expression *expr, UnresolvedId *unresolved_id)
{
    expr->unresolved_id = unresolved_id;
    expr->kind = ExprKind::UnresolvedId;
}

void set_expression(Expression *expr, ValueList *value_list)
{
    expr->value_list = value_list;
    expr->kind = ExprKind::ValueList;
}

// Statements

struct StmtInsert
{
    Expression *subject;
    Expression *insert;
    bool insert_at_end;
};

enum class StmtKind
{
    Expression,
    Insert,
};

struct Statement
{
    StmtKind kind;
    union
    {
        Expression *expr;
        StmtInsert *insert;
    };
};

void set_statement(Statement *stmt, Expression *expr)
{
    stmt->expr = expr;
    stmt->kind = StmtKind::Expression;
}

void set_statement(Statement *stmt, StmtInsert *insert)
{
    stmt->insert = insert;
    stmt->kind = StmtKind::Insert;
}

// Program

struct Scope
{
    Scope *parent;
    Array<Statement *> statements;
};

struct Function
{
    Token identity;
    Scope *scope;
};

struct Program
{
    Array<UnresolvedId> unresolved_ids;
    Array<ValueList> value_lists;
    Array<Expression> expressions;
    Array<StmtInsert> stmt_inserts;
    Array<Statement> statements;
    Array<Scope> scopes;
    Array<Function> functions;
};

// PARSING //

struct Parser
{
    Lexer *lexer;
    Program program;

    Parser(Lexer *lexer) : lexer(lexer) {}
};

void error(const Parser &parser, const char *msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    Token current = parser.lexer->current;
    TokenKind kind = parser.lexer->current.kind; // FIXME: For whatever reason, the value of 'current.kind' is a nonsense value, even when `parser.lexer->current.kind` isn't
    printf("Error on line %d: %s", current.line, msg);
    if (kind == TokenKind::Line)
    {
        printf(" (got new line)\n");
    }
    else
    {
        char value[] = "";
        strncat(value, current.string.first, current.string.length);
        printf(" (got %02d %s)\n", kind, value);
    }
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

Token eat(Parser &parser, TokenKind kind, const char *msg)
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

Expression *parse_expression(Parser &parser)
{
    Expression *expr = yoink(parser.program.expressions);

    printf("PEEK EXPRESSION %0d\n", peek(parser));

    if (peek(parser) == TokenKind::Identity)
    {
        UnresolvedId *unresolved_id = yoink(parser.program.unresolved_ids);
        unresolved_id->identity = eat(parser, TokenKind::Identity, "Expected identity.");
        set_expression(expr, unresolved_id);
    }
    else if (peek(parser) == TokenKind::STRING)
    {
        ValueList *value_list = yoink(parser.program.value_lists);

        Token str = eat(parser, TokenKind::STRING, "Expected string.");
        for (int i = 1; i <= str.string.length - 1; i++)
            append(value_list->values, (int)str.string.first[i]);

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

void parse_statement(Parser &parser, Scope *scope)
{
    Statement *stmt = yoink(parser.program.statements);
    Expression *expr = parse_expression(parser);

    printf("PEEK STATEMENT INFIX %0d\n", peek(parser));

    if (peek(parser) == TokenKind::InsertL || peek(parser) == TokenKind::CurlyR)
    {
        printf("HELLO\n");
        StmtInsert *insert = yoink(parser.program.stmt_inserts);
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

    append(scope->statements, stmt);
}

void parse_scope(Parser &parser, Scope *scope, Scope *parent)
{
    scope->parent = parent;

    bool statement_block = match(parser, TokenKind::CurlyL);

    printf("STATEMENT BLOCK %s\n", statement_block ? "true" : "false");
    if (statement_block)
    {
        while (match(parser, TokenKind::Line))
            ;
        printf("PEEK STATEMENT %s\n", peek_statement(parser) ? "true" : "false");

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
    Function *funct = yoink(parser.program.functions);
    funct->scope = yoink(parser.program.scopes);
    funct->identity = identity;

    eat(parser, TokenKind::ParenL, "Expected '(' after function name.");
    // FIXME: Parse function parameters
    eat(parser, TokenKind::ParenR, "Expected ')' at end of function arguments.");

    parse_scope(parser, funct->scope, NULL);
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

// COMPILE //

int main()
{
    char src[] = "main() {\n\tconsole << \"Hello\"\n}\0";

    Lexer lexer(src);
    Parser parser(&lexer);

    next_token(lexer);
    next_token(lexer);
    parse_program(parser);

    return 0;
}