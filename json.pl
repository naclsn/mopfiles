digit(Char, Num) :- char_code(Char, Code), char_code('0', Zero), Num is Code - Zero, 0 =< Num, Num =< 9.
blanks([' ' | Next], Rest) :- blanks(Next, Rest).
blanks(['\t' | Next], Rest) :- blanks(Next, Rest).
blanks(['\n' | Next], Rest) :- blanks(Next, Rest).
blanks(Rest, Rest).

is_json_null(['n','u','l','l' | Rest], json_null(), Rest).

is_json_boolean(['t','r','u','e' | Rest], json_boolean(true), Rest).
is_json_boolean(['f','a','l','s','e' | Rest], json_boolean(false), Rest).

is_json_number([Digit | Next], json_number(Num), Rest) :-
    digit(Digit, A),
    is_json_number(Next, json_number(B), Rest),
    atom_concat(A, B, Num).
is_json_number([Digit | [NotDigit | Rest]], json_number(Num), [NotDigit | Rest]) :- digit(Digit, Num), \+ digit(NotDigit, _).
is_json_number([Digit], json_number(Num), []) :- digit(Digit, Num).

is_json_string(['"', C | Next], json_string(Str), Rest) :-
    is_json_string(['"' | Next], json_string(B), Rest),
    string_chars(A, [C]),
    string_concat(A, B, Str).
is_json_string(['"', '"' | Rest], json_string(""), Rest).

is_json_list(['[' | Bl_Val_Bl_Sep_Next], json_list([H | T]), Rest) :-
    blanks(Bl_Val_Bl_Sep_Next, Val_Bl_Sep_Next),
    is_json_value(Val_Bl_Sep_Next, H, Bl_Sep_Next),
    blanks(Bl_Sep_Next, [',' | Next]),
    is_json_list(['[' | Next], json_list(T), Rest).
is_json_list(['[' | Bl_Val_Bl_Rest], json_list([E]), Rest) :-
    blanks(Bl_Val_Bl_Rest, Val_Bl_Rest),
    is_json_value(Val_Bl_Rest, E, Bl_Rest),
    blanks(Bl_Rest, [']' | Rest]).
is_json_list(['[' | Bl_Rest], json_list([]), Rest) :- blanks(Bl_Rest, [']' | Rest]).

is_json_record(['{' | Bl_Key_Bl_Col_Bl_Val_Bl_Sep_Next], json_record([Key:Val | T]), Rest) :-
    blanks(Bl_Key_Bl_Col_Bl_Val_Bl_Sep_Next, Key_Bl_Col_Bl_Val_Bl_Sep_Next),
    is_json_string(Key_Bl_Col_Bl_Val_Bl_Sep_Next, json_string(Key), Bl_Col_Bl_Val_Bl_Sep_Next),
    blanks(Bl_Col_Bl_Val_Bl_Sep_Next, [':' | Bl_Val_Bl_Sep_Next]),
    blanks(Bl_Val_Bl_Sep_Next, Val_Bl_Sep_Next),
    is_json_value(Val_Bl_Sep_Next, Val, Bl_Sep_Next),
    blanks(Bl_Sep_Next, [',' | Next]),
    is_json_record(['{' | Next], json_record(T), Rest).
is_json_record(['{' | Bl_Key_Bl_Col_Bl_Val_Bl_Sep_Next], json_record([Key:Val]), Rest) :-
    blanks(Bl_Key_Bl_Col_Bl_Val_Bl_Sep_Next, Key_Bl_Col_Bl_Val_Bl_Sep_Next),
    is_json_string(Key_Bl_Col_Bl_Val_Bl_Sep_Next, json_string(Key), Bl_Col_Bl_Val_Bl_Sep_Next),
    blanks(Bl_Col_Bl_Val_Bl_Sep_Next, [':' | Bl_Val_Bl_Sep_Next]),
    blanks(Bl_Val_Bl_Sep_Next, Val_Bl_Sep_Next),
    is_json_value(Val_Bl_Sep_Next, Val, Bl_Sep_Next),
    blanks(Bl_Sep_Next, ['}' | Rest]).
is_json_record(['{' | Bl_Rest], json_record([]), Rest) :- blanks(Bl_Rest, ['}' | Rest]).

is_json_value(Repr, Obj, Rest) :- is_json_null(Repr, Obj, Rest).
is_json_value(Repr, Obj, Rest) :- is_json_boolean(Repr, Obj, Rest).
is_json_value(Repr, Obj, Rest) :- is_json_number(Repr, Obj, Rest).
is_json_value(Repr, Obj, Rest) :- is_json_string(Repr, Obj, Rest).
is_json_value(Repr, Obj, Rest) :- is_json_list(Repr, Obj, Rest).
is_json_value(Repr, Obj, Rest) :- is_json_record(Repr, Obj, Rest).

json(String, Object) :- string_chars(String, Chars), is_json_value(Chars, Object, []).
