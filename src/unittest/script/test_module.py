# Some simple test objects to see if eveything works

def another_test_function():
  print "Hello from another_test_function"

def test_function(text=None, number=0):
  print number
  if text:
    print text
  else:
    print "Hello from test_func"
  #return "Test return", ( 4, 6), {"2": "String 2", 2: "Integer 2"}, test_function
  return another_test_function
