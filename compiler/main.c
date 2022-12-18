#include <stdio.h>
#include <stdbool.h>
#include <cstring>

// STRING //

typedef struct
{
    const char *first;
    int length;
} str;

// ERROR HANDLING //
bool error_has_occoured = false;

// TOKENS //

typedef enum
{
    TOKEN_NULL,

    TOKEN_IDENTITY,
    TOKEN_CURLY_L,
    TOKEN_CURLY_R,
    TOKEN_PAREN_L,
    TOKEN_PAREN_R,

    TOKEN_STRING,

    TOKEN_INSERT_L,
    TOKEN_INSERT_R,
    TOKEN_LESS_THAN,
    TOKEN_LESS_THAN_EQUAL,
    TOKEN_GREATER_THAN,
    TOKEN_GREATER_THAN_EQUAL,

    TOKEN_LINE,
    TOKEN_EOF,
} Token_Kind;

typedef struct
{
    Token_Kind kind;
    str string;
    int line;
} Token;

Token null_token = {
    TOKEN_NULL,
    {NULL, 0},
    0,
};

// LEXING //

typedef struct
{
    const char *src;
    const char *token_first;
    const char *current_char;
    int current_line;
    Token current;
    Token next;
} Lexer;

Lexer create_lexer(const char *src)
{
    Lexer lexer;
    lexer.src = src;
    lexer.token_first = lexer.src;
    lexer.current_char = lexer.src;
    lexer.current_line = 1;
    lexer.current = null_token;
    lexer.next = null_token;
    return lexer;
}

void error(Lexer *lexer, const char *msg)
{
    if (error_has_occoured)
        return;

    printf("Error on line %d: %s\n", lexer->current_line, msg);
    error_has_occoured = true;
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

void make_token(Lexer *lexer, Token_Kind kind)
{
    lexer->next.kind = kind;
    lexer->next.string.first = lexer->token_first;
    lexer->next.string.length = (int)(lexer->current_char - lexer->token_first);
    lexer->next.line = kind != TOKEN_LINE ? lexer->current_line : lexer->current_line - 1;
}

void next_token(Lexer *lexer)
{
    lexer->current = lexer->next;

    char value[] = "";
    if (lexer->current.kind == TOKEN_LINE)
        strcat(value, "new line");
    else
        strncat(value, lexer->current.string.first, lexer->current.string.length);
    printf("%02d %s\n", lexer->current.kind, value);

    if (lexer->next.kind == TOKEN_EOF || lexer->current.kind == TOKEN_EOF)
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
            make_token(lexer, TOKEN_LINE);
            return;

        case '(':
            make_token(lexer, TOKEN_PAREN_L);
            return;
        case ')':
            make_token(lexer, TOKEN_PAREN_R);
            return;
        case '{':
            make_token(lexer, TOKEN_CURLY_L);
            return;
        case '}':
            make_token(lexer, TOKEN_CURLY_R);
            return;

        case '<':
        {
            switch (peek(lexer))
            {
            case '<':
                next(lexer);
                make_token(lexer, TOKEN_INSERT_L);
                return;
            case '=':
                next(lexer);
                make_token(lexer, TOKEN_LESS_THAN_EQUAL);
                return;
            }
            make_token(lexer, TOKEN_LESS_THAN);
            return;
        }

        case '>':
        {
            switch (peek(lexer))
            {
            case '>':
                next(lexer);
                make_token(lexer, TOKEN_INSERT_R);
                return;
            case '=':
                next(lexer);
                make_token(lexer, TOKEN_GREATER_THAN_EQUAL);
                return;
            }
            make_token(lexer, TOKEN_GREATER_THAN);
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
            make_token(lexer, TOKEN_STRING);
            return;
        }

        default:
        {
            if (char_is_name(c))
            {
                while (char_is_name(peek(lexer)))
                    next(lexer);
                make_token(lexer, TOKEN_IDENTITY);
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
    make_token(lexer, TOKEN_EOF);
}

// PROGRAM MODEL //

typedef struct
{
    Token identity;
} Function;

typedef struct
{
    Function functions[256];
    int function_count;
} Program;

// PARSING //

typedef struct
{
    Lexer *lexer;
    Program program;
} Parser;

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

    Token current = parser->lexer->current;
    char value[] = "";
    if (current.kind == TOKEN_LINE)
        strcat(value, "new line");
    else
        strncat(value, current.string.first, current.string.length);
    printf("Error on line %d: %s (got %02d %s)\n", current.line, msg, current.kind, value);
    error_has_occoured = true;
}

Token_Kind peek(Parser *parser)
{
    return parser->lexer->current.kind;
}

Token_Kind peek_next(Parser *parser)
{
    return parser->lexer->next.kind;
}

bool match(Parser *parser, Token_Kind kind)
{
    if (peek(parser) != kind)
        return false;
    next_token(parser->lexer);
    return true;
}

Token eat(Parser *parser, Token_Kind kind, const char *msg)
{
    if (kind != TOKEN_LINE)
    {
        while (match(parser, TOKEN_LINE))
            ;
    }

    Token t = parser->lexer->current;
    if (!match(parser, kind))
        error(parser, msg);
    return t;
}

void parse_function(Parser *parser, Token identity)
{
    Function *funct = &parser->program.functions[parser->program.function_count];
    parser->program.function_count++;

    funct->identity = identity;

    eat(parser, TOKEN_PAREN_L, "Expected '(' after function name.");
    // FIXME: Parse function parameters
    eat(parser, TOKEN_PAREN_R, "Expected ')' at end of function arguments.");

    eat(parser, TOKEN_CURLY_L, "Expected '{' after function declaration.");
    // FIXME: Parse function body
    eat(parser, TOKEN_CURLY_R, "Expected '}' at end of function.");
}

void parse_program(Parser *parser)
{
    parser->program.function_count = 0;

    while (peek(parser) == TOKEN_IDENTITY)
    {
        Token identity = eat(parser, TOKEN_IDENTITY, "Expected function or variable declaration.");
        if (peek(parser) == TOKEN_PAREN_L)
        {
            parse_function(parser, identity);
            continue;
        }
        error(parser, "Expected function or variable declaration.");
    }
}

// COMPILE //

int main()
{
    const char *src = "main() {\n\tconsole << \"Hello\"\n}\0";

    Lexer lexer = create_lexer(src);
    Parser parser = create_parser(&lexer);

    next_token(&lexer);
    next_token(&lexer);
    parse_program(&parser);

    return 0;
}