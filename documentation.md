> ⚠️ Documentation not yet written. This document currently just contains notes to help keep track of some of the language details until the docs are written.

# Notes

## Insertion operator mechanics

| A     | >>  | B     |                                                   |
| ----- | --- | ----- | ------------------------------------------------- |
| value | >>  | value | Invalid.                                          |
| list  | >>  | value | Pop from the end of A and store in B.             |
| value | >>  | list  | Append A to the front of B.                       |
| list  | >>  | list  | Pop and append all values of A to the front of B. |
| value | <<  | value | Invalid.                                          |
| list  | <<  | value | Append B to the end of A.                         |
| value | <<  | list  | Pop from the front of B and store in A.           |
| list  | <<  | list  | Pop and append all values of B to the end of A.   |
