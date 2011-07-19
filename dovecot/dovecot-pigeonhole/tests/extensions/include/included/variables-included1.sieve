require "include";
require "variables";

global ["value1"];
global ["result1"];

set "result1" "${value1} ${global.value2}";
