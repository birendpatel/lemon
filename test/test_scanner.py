# Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
# The tokenization test captures the token output from stderr when Lemon
# is executed with the --Dtokens flag. The token types are extracted from
# the captured output and compared against the expected result.

import unittest

from re import search
from subprocess import run

class TestTokenizer(unittest.TestCase):
    def test_all_tokenization_cases_from_file_input(self):
        #arrange
        self.maxDiff = None

        cmd = "../debug/lemon --Dtokens ./test_scanner.lem"

        expected = [
            "IDENTIFIER",
            "IDENTIFIER",
            "IDENTIFIER",
            "IDENTIFIER",
            "SEMICOLON",
            "INT LITERAL",
            "INT LITERAL",
            "INT LITERAL",
            "INT LITERAL",
            "SEMICOLON",
            "FLOAT LITERAL",
            "FLOAT LITERAL",
            "FLOAT LITERAL",
            "FLOAT LITERAL",
            "FLOAT LITERAL",
            "FLOAT LITERAL",
            "SEMICOLON",
            "STRING LITERAL",
            "STRING LITERAL",
            "STRING LITERAL",
            "SEMICOLON",
            "COLON",
            "LEFT BRACKET",
            "RIGHT BRACKET",
            "LEFT PARENTHESIS",
            "RIGHT PARENTHESIS",
            "LEFT BRACE",
            "RIGHT BRACE",
            "DOT",
            "TILDE",
            "COMMA",
            "SEMICOLON",
            "EQUAL",
            "EQUAL EQUAL",
            "NOT EQUAL",
            "NOT",
            "AND",
            "OR",
            "BITWISE NOT",
            "AMPERSAND",
            "BITWISE OR",
            "BITWISE XOR",
            "LEFT SHIFT",
            "RIGHT SHIFT",
            "GREATER THAN",
            "GREATER OR EQUAL",
            "LESS THAN",
            "LESS OR EQUAL",
            "ADD",
            "MINUS",
            "STAR",
            "DIVISION",
            "MODULO",
            "FOR LOOP",
            "WHILE LOOP",
            "BREAK",
            "CONTINUE",
            "IF BRANCH",
            "ELSE BRANCH",
            "SWITCH",
            "CASE",
            "DEFAULT",
            "FALLTHROUGH",
            "GOTO",
            "LET",
            "MUT",
            "NULL",
            "BOOL TRUE",
            "BOOL FALSE",
            "STRUCT",
            "IMPORT",
            "FUNC",
            "PRIVATE",
            "PUBLIC",
            "RETURN",
            "SELF",
            "VOID",
            "EQUAL EQUAL", "EQUAL",
            "NOT EQUAL", "EQUAL",
            "NOT EQUAL", "EQUAL EQUAL", "EQUAL",
            "AND", "AMPERSAND",
            "OR", "BITWISE OR",
            "LEFT SHIFT", "LESS THAN",
            "RIGHT SHIFT", "GREATER THAN",
            "LEFT SHIFT", "LESS OR EQUAL",
            "RIGHT SHIFT", "GREATER OR EQUAL",
            "GREATER OR EQUAL", "EQUAL EQUAL",
            "GREATER OR EQUAL", "EQUAL EQUAL", "EQUAL",
            "LESS OR EQUAL", "EQUAL EQUAL",
            "LESS OR EQUAL", "EQUAL EQUAL", "EQUAL"
        ]

        #act
        proc_output = run(cmd, check=True, shell=True, capture_output=True)
        text = proc_output.stderr.decode()
        types = self.isolate(text)

        #assert
        self.assertListEqual(types, expected)
    
    def isolate(self, text: str):
        """return a list of token types in order of appearance"""
        lines = text.split('\n')

        tokens = [i for i in lines if i.find("TOKEN") != -1 ]

        types = []

        # fetch the region of text in the token which is between the two colons
        # and then strip those colons along with any inner whitespace
        for i in tokens:
            match = search(r":[^:]+:", i)

            if match:
                types.append(match.group()[1:-1].strip())

        return types


if __name__ == "__main__":
    unittest.main(verbosity=2)
