from lang.conf import DEBUG

def parse(unparsed, depth=0):
    """
    Parse nested S-expressions. Raises for unbalanced parens.
    Returns a tuple of (unparsed, AST).
    The top level is always a list.
    """
    ast = []
    inside_comment = False
    inside_word = False
    inside_string = False
    while unparsed:
        c = unparsed[0]
        unparsed = unparsed[1:]
        if inside_comment:
            if '\n' == c:
                inside_comment = False
        elif '\\' == c and not inside_string:
            for literal in ['space', 'tab', 'newline']:
                # Some literals are multiple chars
                if unparsed.startswith(literal):
                    ast += [c + unparsed[:len(literal)]]
                    unparsed = unparsed[len(literal):]
                    break
            else:
                # Else just take a char and call it done
                ast += [c + unparsed[0]]
                unparsed = unparsed[1:]
        elif ';' == c:
            inside_comment = True
        elif '"' == c:
            if inside_string:
                ast[-1] += c
            else:
                ast += [c]
            inside_string = not inside_string
        elif inside_string:
            ast[-1] += c
        elif '(' == c:
            unparsed, inner_ast = parse(unparsed, depth=depth+1)
            ast += [inner_ast]
        elif ')' == c:
            if depth == 0:
                raise Exception('Unexpected ) while parsing')
            return unparsed, ast
        elif c.isspace():
            inside_word = False
        elif inside_word:
            ast[-1] += c
        else:
            inside_word = True
            ast += c
    if depth != 0:
        raise Exception('Unexpected EOF while parsing')
    if DEBUG:
        from pprint import pprint
        pprint(ast)
    return unparsed, ast
