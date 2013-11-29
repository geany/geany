# simple single line comment
from somemodule import proc1, proc2
import module3

type TWord = object of TObject
  word: string
  count: int

type TComplexWord = object of TWord
  tag: string

proc countWord(par: string): seq[TWord] =
  ## count words in paragraph
  result = @[]
  var underFunction = "someString"
  for word in split(par):
    result.add(TWord(count: 1, word: word))
  return result

const str1 = "simple string"
var str2 = """Triple string"""
let str3 = r"raw string"
if True:
  let str4 = "aaa"
let someInt = 999

# basic iterator
iterator countup(a, b: int): int =
  var res = a
  while res <= b:
    yield res
    inc(res)
