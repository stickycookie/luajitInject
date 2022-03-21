print("inject successfully")
--_G["OldFun"] = _G["Add1"]
function Add1(a,b,c,d,e,f,g)
	print("in inject function")
	OldFun = _G["OldFun"]
	a = a*999
	return OldFun(a,b,c,d,e,f,g)
end
function TestFun()
	print("test in inject")
	TestMain()
end
--local a = {1,2,3,4,5,6,7}
--print(Add1(1,2,3,4,5,6,7))