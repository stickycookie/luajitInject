#include <iostream>
#include <vector>
using namespace std;

unsigned char readBuffer[1024000];
unsigned char* filePtr;
int fileSize;

struct ProtoHeader
{
	int protoSize, complexCnt, numericCnt, instructionCnt;
	unsigned char flags, argCnt, frameSize, upValueCnt;
};
struct Proto
{
	ProtoHeader ph;
	unsigned char* instructions, * constants;
	int instructionsSize, constantsSize;
};

int ReadUleb128(unsigned char*& buf)
{
	unsigned int result;
	unsigned char cur;

	result = *buf++;
	if (result > 0x7f)
	{
		cur = *buf++;
		result = (result & 0x7f) | (unsigned int)((cur & 0x7f) << 7);
		if (cur > 0x7f)
		{
			cur = *buf++;
			result |= (unsigned int)(cur & 0x7f) << 14;
			if (cur > 0x7f)
			{
				cur = *buf++;
				result |= (unsigned int)(cur & 0x7f) << 21;
				if (cur > 0x7f)
				{
					cur = *buf++;
					result |= (unsigned int)cur << 28;
				}
			}
		}
	}
	return result;
}
unsigned char ReadU8(unsigned char*& buf)
{
	return *buf++;
}
void WriteU8(unsigned char*& buf, unsigned char x)
{
	*buf = x;
	buf++;
}
void WriteUleb128(unsigned char*& buf, int x)
{
	unsigned int denominator = 0x80;
	unsigned char flag = 0x80;

	for (int i = 0; i < 5; i++)
	{
		if (x < denominator)
		{
			buf[i] = (unsigned char)x;
			i++;
			buf += i;
			return;
		}
		buf[i] = (flag) | (unsigned char)(x % denominator);
		x = x >> 7;
	}
}
void WriteString(unsigned char*& buf, string x)
{
	*buf = x.length() + 5;
	buf++;
	memcpy(buf, x.c_str(), x.length());
	buf += x.length();
}

void IngoreSeg(unsigned char*& buf, int n)
{
	while (n--)
	{
		int size = ReadUleb128(buf);
		buf += size;
	}
}
Proto* ReadProto(unsigned char* buf)
{
	unsigned char* ed = buf;
	int size = ReadUleb128(ed);
	ed += size;

	Proto* proto = new Proto;
	proto->ph.protoSize = ReadUleb128(buf);
	proto->ph.flags = ReadU8(buf);
	proto->ph.argCnt = ReadU8(buf);
	proto->ph.frameSize = ReadU8(buf);
	proto->ph.upValueCnt = ReadU8(buf);
	proto->ph.complexCnt = ReadUleb128(buf);
	proto->ph.numericCnt = ReadUleb128(buf);
	proto->ph.instructionCnt = ReadUleb128(buf);

	proto->instructionsSize = 4 * proto->ph.instructionCnt;
	proto->instructions = new unsigned char[proto->instructionsSize];
	memcpy(proto->instructions, buf, proto->instructionsSize);
	buf += 4 * proto->ph.instructionCnt;

	proto->constantsSize = ed - buf;
	proto->constants = new unsigned char[proto->constantsSize];
	memcpy(proto->constants, buf, proto->constantsSize);
	buf += proto->constantsSize;
	return proto;
}
void InsertInstruction(Proto* proto, string& name)
{
	unsigned char* tmp;

	proto->ph.complexCnt += 2;
	tmp = new unsigned char[proto->constantsSize + 64];
	unsigned char* st = tmp;
	WriteString(tmp, string("require"));
	WriteString(tmp, name);
	memcpy(tmp, proto->constants, proto->constantsSize);
	proto->constantsSize += tmp - st;
	proto->constants = st;

	//G[complexCnt-1]="require" G[complexCnt-2]="inject"
	tmp = new unsigned char[proto->instructionsSize + 12];
	memcpy(tmp + 12, proto->instructions, proto->instructionsSize);
	unsigned char injectCode[] = { 0x36, 0x00, 0x00, 0x00,
									0x27, 0x01, 0x01, 0x00,
									0x42, 0x00, 0x02, 0x01 };
	injectCode[2] = proto->ph.complexCnt - 1;
	injectCode[6] = proto->ph.complexCnt - 2;
	memcpy(tmp, injectCode, 12);
	proto->instructions = tmp;
	proto->ph.instructionCnt += 3;
	proto->instructionsSize += 3 * 4;
}
void CalcProtoSize(Proto* proto)
{
	unsigned char* buf = new unsigned char[4096];
	unsigned char* st = buf;

	WriteU8(buf, proto->ph.flags);
	WriteU8(buf, proto->ph.argCnt);
	WriteU8(buf, proto->ph.frameSize);
	WriteU8(buf, proto->ph.upValueCnt);
	WriteUleb128(buf, proto->ph.complexCnt);
	WriteUleb128(buf, proto->ph.numericCnt);
	WriteUleb128(buf, proto->ph.instructionCnt);
	proto->ph.protoSize = (buf - st) + proto->constantsSize + proto->instructionsSize;
}
void WriteProtoIntoBuffer(unsigned char*& buf, Proto* proto)
{
	WriteUleb128(buf, proto->ph.protoSize);
	WriteU8(buf, proto->ph.flags);
	WriteU8(buf, proto->ph.argCnt);
	WriteU8(buf, proto->ph.frameSize);
	WriteU8(buf, proto->ph.upValueCnt);
	WriteUleb128(buf, proto->ph.complexCnt);
	WriteUleb128(buf, proto->ph.numericCnt);
	WriteUleb128(buf, proto->ph.instructionCnt);

	memcpy(buf, proto->instructions, proto->instructionsSize);
	buf += proto->instructionsSize;
	memcpy(buf, proto->constants, proto->constantsSize);
	buf += proto->constantsSize;
}
int GetSegCnt(unsigned char* buf)
{
	int cnt = 0;
	while (true)
	{
		int size = ReadUleb128(buf);
		if (size == 0) break;
		buf += size;
		cnt++;
	}
	return cnt;
}

int main()
{
	FILE* f = fopen("D:\\笔记\\luajitHook\\v2\\target", "rb");
	fileSize = fread(readBuffer, 1, 1024000, f);
	fclose(f);

	string name = "inject";
	filePtr = readBuffer + 5;
	int targetSeg = GetSegCnt(filePtr) - 1;
	IngoreSeg(filePtr, targetSeg);
	unsigned char* oriBound = filePtr;
	Proto* proto = ReadProto(filePtr);
	filePtr = oriBound;
	InsertInstruction(proto, name);
	CalcProtoSize(proto);
	WriteProtoIntoBuffer(filePtr, proto);
	WriteU8(filePtr, 0);

	f = fopen("D:\\笔记\\luajitHook\\v2\\target_mod", "wb");
	fwrite(readBuffer, 1, filePtr - readBuffer, f);
	fclose(f);
	return 0;
}