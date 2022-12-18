# 🐛 tiny

A tiny language with as few features as I could manage.
(but with syntax sugar to spare!)

## Samples

### Hello World

```tiny
main() {
    console << "Hello"
}
```

### Graph calculator

```tiny
f(x, y) = 10sin(x/10) - y

inflection(x, y) {
    r = f(x + 0.5, y) > 0
    l = f(x - 0.5, y) > 0
    u = f(x, y + 0.5) > 0
    d = f(x, y - 0.5) > 0
    return r != l | u != d
}

size = 30

main() {
    for (y, -size..size) {
        for (x, -size..size) {
            console << if (inflection(x, y)) "X"
                elseif (x == 0 & y == 0) "+"
                elseif (x == 0) "-"
                elseif (y == 0) "|"
                else " "
        }
        console << "\n"
    }
}
```
