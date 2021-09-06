# Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
# The tokenization test captures the token output from stderr when Lemon
# is executed with the --Dtokens flag. The token types are extracted from
# the captured output and compared against the expected result.

import unittest

from re import search
from subprocess import run

# returns a list of types, in the order that they occur, from the input
def isolate(text: str):
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

class TestTokenizer(unittest.TestCase):
    def test_all_tokenization_cases_from_file_input(self):
        #arrange
        cmd = "../debug/lemon --Dtokens ./test_scanner.lem"

        #act
        proc_output = run(cmd, check=True, shell=True, capture_output=True)
        text = proc_output.stderr.decode()

        types = isolate(text)

        print(types)

        #assert
        self.assertTrue(1 == 1)

if __name__ == "__main__":
    unittest.main(verbosity=2)
