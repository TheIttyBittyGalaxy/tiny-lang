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

enum class TOKEN
{
    NULL_TOKEN,

    IDENTITY,
    CURLY_L,
    CURLY_R,
    PAREN_L,
    PAREN_R,

    STRING,

    INSERT_L,
    INSERT_R,
    LESS_THAN,
    LESS_THAN_EQUAL,
    GREATER_THAN,
    GREATER_THAN_EQUAL,

    LINE,
    END_OF_FILE,
};

struct Token
{
    TOKEN kind = TOKEN::NULL_TOKEN;
    str string = {nullptr, 0};
    int line = 0;
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
};

Lexer create_lexer(const char *src)
{
    Lexer lexer;
    lexer.src = src;
    lexer.token_first = lexer.src;
    lexer.current_char = lexer.src;
    lexer.current_line = 1;
    return lexer;
}

void error(Lexer *lexer, const char *msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    printf("Error on line %d: %s\n", lexer->current_line, msg);
}

char peek(Lexer *lexer)
{
    return *lexer->current_char;
}

char peek(Lexer *lexer, int ahead)
{
    return peek(lexer) == '\0' ? '\0' : *(lexer->current_char + ahead);
}

char next(Lexer *lexer)
{
    char c = peek(lexer);
    lexer->current_char++;
    if (c == '\n')
        lexer->current_line++;
    return c;
}

bool match(Lexer *lexer, char c)
{
    if (peek(lexer) != c)
        return false;
    next(lexer);
    return true;
}

bool char_is_name(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void make_token(Lexer *lexer, TOKEN kind)
{
    lexer->next.kind = kind;
    lexer->next.string.first = lexer->token_first;
    lexer->next.string.length = (int)(lexer->current_char - lexer->token_first);
    lexer->next.line = kind != TOKEN::LINE ? lexer->current_line : lexer->current_line - 1;
}

void next_token(Lexer *lexer)
{
    lexer->current = lexer->next;

    char value[] = "";
    if (lexer->current.kind == TOKEN::LINE)
        strcat(value, "new line");
    else
        strncat(value, lexer->current.string.first, lexer->current.string.length);
    printf("%02d %s\n", lexer->current.kind, value);

    if (lexer->next.kind == TOKEN::END_OF_FILE || lexer->current.kind == TOKEN::END_OF_FILE)
        return;

    while (peek(lexer) != '\0')
    {
        while (peek(lexer) == ' ' || peek(lexer) == '\t')
            next(lexer);

        lexer->token_first = lexer->current_char;

        char c = next(lexer);
        switch (c)
        {
        case '\n':
            make_token(lexer, TOKEN::LINE);
            return;

        case '(':
            make_token(lexer, TOKEN::PAREN_L);
            return;
        case ')':
            make_token(lexer, TOKEN::PAREN_R);
            return;
        case '{':
            make_token(lexer, TOKEN::CURLY_L);
            return;
        case '}':
            make_token(lexer, TOKEN::CURLY_R);
            return;

        case '<':
        {
            switch (peek(lexer))
            {
            case '<':
                next(lexer);
                make_token(lexer, TOKEN::INSERT_L);
                return;
            case '=':
                next(lexer);
                make_token(lexer, TOKEN::LESS_THAN_EQUAL);
                return;
            }
            make_token(lexer, TOKEN::LESS_THAN);
            return;
        }

        case '>':
        {
            switch (peek(lexer))
            {
            case '>':
                next(lexer);
                make_token(lexer, TOKEN::INSERT_R);
                return;
            case '=':
                next(lexer);
                make_token(lexer, TOKEN::GREATER_THAN_EQUAL);
                return;
            }
            make_token(lexer, TOKEN::GREATER_THAN);
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
            make_token(lexer, TOKEN::STRING);
            return;
        }

        default:
        {
            if (char_is_name(c))
            {
                while (char_is_name(peek(lexer)))
                    next(lexer);
                make_token(lexer, TOKEN::IDENTITY);
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

    lexer->token_first = lexer->current_char;
    make_token(lexer, TOKEN::END_OF_FILE);
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

enum ExprKind
{
    EXPR_UNRESOLVED_ID,
    EXPR_VALUE_LIST,
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
    expr->kind = EXPR_UNRESOLVED_ID;
}

void set_expression(Expression *expr, ValueList *value_list)
{
    expr->value_list = value_list;
    expr->kind = EXPR_VALUE_LIST;
}

// Statements

struct StmtInsert
{
    Expression *subject;
    Expression *insert;
    bool insert_at_end;
};

enum StmtKind
{
    STMT_EXPR,
    STMT_INSERT,
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
    stmt->kind = STMT_EXPR;
}

void set_statement(Statement *stmt, StmtInsert *insert)
{
    stmt->insert = insert;
    stmt->kind = STMT_INSERT;
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
};

Parser create_parser(Lexer *lexer)
{
    Parser parser;
    parser.lexer = lexer;
    return parser;
}

void error(Parser *parser, const char *msg)
{
    if (error_has_occoured)
        return;
    error_has_occoured = true;

    Token current = parser->lexer->current;
    TOKEN kind = parser->lexer->current.kind; // FIXME: For whatever reason, the value of 'current.kind' is a nonsense value, even when `parser->lexer->current.kind` isn't
    printf("Error on line %d: %s", current.line, msg);
    if (kind == TOKEN::LINE)
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

TOKEN peek(Parser *parser)
{
    return parser->lexer->current.kind;
}

TOKEN peek_next(Parser *parser)
{
    return parser->lexer->next.kind;
}

bool match(Parser *parser, TOKEN kind)
{
    if (peek(parser) != kind)
        return false;
    next_token(parser->lexer);
    return true;
}

Token eat(Parser *parser, TOKEN kind, const char *msg)
{
    if (kind != TOKEN::LINE)
    {
        while (match(parser, TOKEN::LINE))
            ;
    }

    Token t = parser->lexer->current;
    if (!match(parser, kind))
        error(parser, msg);
    return t;
}

bool peek_expression(Parser *parser)
{
    return peek(parser) == TOKEN::IDENTITY ||
           peek(parser) == TOKEN::STRING;
}

Expression *parse_expression(Parser *parser)
{
    Expression *expr = yoink(parser->program.expressions);

    printf("PEEK EXPRESSION %0d\n", peek(parser));

    if (peek(parser) == TOKEN::IDENTITY)
    {
        UnresolvedId *unresolved_id = yoink(parser->program.unresolved_ids);
        unresolved_id->identity = eat(parser, TOKEN::IDENTITY, "Expected identity.");
        set_expression(expr, unresolved_id);
    }
    else if (peek(parser) == TOKEN::STRING)
    {
        ValueList *value_list = yoink(parser->program.value_lists);

        Token str = eat(parser, TOKEN::STRING, "Expected string.");
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

bool peek_statement(Parser *parser)
{
    return peek_expression(parser);
}

void parse_statement(Parser *parser, Scope *scope)
{
    Statement *stmt = yoink(parser->program.statements);
    Expression *expr = parse_expression(parser);

    printf("PEEK STATEMENT INFIX %0d\n", peek(parser));

    if (peek(parser) == TOKEN::INSERT_L || peek(parser) == TOKEN::CURLY_R)
    {
        printf("HELLO\n");
        StmtInsert *insert = yoink(parser->program.stmt_inserts);
        insert->subject = expr;

        if (match(parser, TOKEN::INSERT_L))
            insert->insert_at_end = true;
        else if (match(parser, TOKEN::INSERT_R))
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

void parse_scope(Parser *parser, Scope *scope, Scope *parent)
{
    scope->parent = parent;

    bool statement_block = match(parser, TOKEN::CURLY_L);

    printf("STATEMENT BLOCK %s\n", statement_block ? "true" : "false");
    if (statement_block)
    {
        while (match(parser, TOKEN::LINE))
            ;
        printf("PEEK STATEMENT %s\n", peek_statement(parser) ? "true" : "false");

        while (peek_statement(parser))
        {
            parse_statement(parser, scope);
            eat(parser, TOKEN::LINE, "Expected newline to terminate statement");
            while (match(parser, TOKEN::LINE))
                ;
        }

        // FIXME: Give the line number of the '{' we are trying to close
        eat(parser, TOKEN::CURLY_R, "Expected '}' to close block.");
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

void parse_function(Parser *parser, Token identity)
{
    Function *funct = yoink(parser->program.functions);
    funct->scope = yoink(parser->program.scopes);
    funct->identity = identity;

    eat(parser, TOKEN::PAREN_L, "Expected '(' after function name.");
    // FIXME: Parse function parameters
    eat(parser, TOKEN::PAREN_R, "Expected ')' at end of function arguments.");

    parse_scope(parser, funct->scope, NULL);
}

void parse_program(Parser *parser)
{
    while (peek(parser) == TOKEN::IDENTITY)
    {
        Token identity = eat(parser, TOKEN::IDENTITY, "Expected function or variable declaration.");
        if (peek(parser) == TOKEN::PAREN_L)
        {
            parse_function(parser, identity);
            continue;
        }
        error(parser, "Expected function or variable declaration.");
    }

    eat(parser, TOKEN::END_OF_FILE, "Expected end of file");
}

// COMPILE //

int main()
{
    char src[] = "main() {\n\tconsole << \"Hello\"\n}\0";

    Lexer lexer = create_lexer(src);
    Parser parser = create_parser(&lexer);

    next_token(&lexer);
    next_token(&lexer);
    parse_program(&parser);

    return 0;
}