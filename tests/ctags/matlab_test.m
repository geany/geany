  %{  
Block comments should be ignored
function FAIL1
    %{  
        Block comments can be nested
    %}  
function FAIL2
  %}  
function [x] = func0
% The line below is just a regular comment; it's not closing any block
%}
function [x,y,z] = func1 
function x = func2 
function func3 
function y = func4(a, b, c)
function func5   % this comment should be ignored --> X = FAIL5
functionality = FAIL6; % this is not a function and should not be parsed

