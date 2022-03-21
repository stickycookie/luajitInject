function Test(x)
	print("Test " .. x)
end

Test("enter main==>test()")
require("addLib")

function fun1()
	print("Test in target.lua fun1 function...")
end
function fun2()
	print("Test in target.lua fun2 function...")
end

print("Test in target.lua main function...")
print(add(1,2))