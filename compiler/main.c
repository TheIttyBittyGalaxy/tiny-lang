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
            strncat(msg, "'", 1);
            error(lexer, msg);
            return;
        }
        }
    }

    lexer->token_first = lexer->current_char;
    make_token(lexer, TOKEN_EOF);
}

// COMPILE //

int main()
{
    const char *src = "main() {\n\tconsole << \"Hello\"\n}\0";
    Lexer lexer = create_lexer(src);

    next_token(&lexer);
    next_token(&lexer);

    while (lexer.current.kind != TOKEN_EOF)
    {
        char value[] = "";
        if (lexer.current.kind != TOKEN_LINE)
            strncat(value, lexer.current.string.first, lexer.current.string.length);

        printf("%d %s\n", lexer.current.kind, value);

        next_token(&lexer);
        if (error_has_occoured)
            break;
    }

    return 0;
}