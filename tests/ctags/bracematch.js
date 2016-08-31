/*
 * "Braces aren't properly matched in findCmdTerm()"
 * 
 * ctags -f - bracematch.js should output:
 * 
 * functions:
 *    Container
 *    Container.x
 *    Container.y
 * 
 * classes:
 *    MyClass
 * 
 * methods:
 *    MyClass.insert
 *    MyClass.wrap
 */


function Container() {
  function x() {
    if (!x)
      x = { };
  }

  function y() {
    
  }
}

// from prototype.js, a lot simplified to only show the issue
MyClass = {
  insert: function(element, insertions) {
    element = $(element);

    if (condition)
      insertions = {bottom:insertions};

    var content, insert, tagName, childNodes;
  },

  wrap: function(element, wrapper, attributes) {
    // ...
  }
}
