#ifndef PACKET_INCLUDE_H
#define PACKET_INCLUDE_H

#include <stdint.h>
#include <memory>
#include <assert.h>
#include <muduo/net/Buffer.h>

#include <google/protobuf/message.h>

typedef std::shared_ptr<muduo::net::Buffer> BufferPtr;

#define SESSIONSZ  32
#define AESKEYSZ   16
#define SERVIDSZ   50

#define HEADER_SIGN          (0x5F5F)
#define PROTO_BUF_SIGN       (0xF5F5F5F5)

namespace packet {

#pragma pack(1)

	//@@ enctypeE ��������
	enum enctypeE {
		PUBENC_JSON_NONE         = 0x01,
		PUBENC_PROTOBUF_NONE     = 0x02,
		PUBENC_JSON_BIT_MASK     = 0x11,
		PUBENC_PROTOBUF_BIT_MASK = 0x12,
		PUBENC_JSON_RSA          = 0x21,
		PUBENC_PROTOBUF_RSA      = 0x22,
		PUBENC_JSON_AES          = 0x31,
		PUBENC_PROTOBUF_AES      = 0x32,
	};

	//@@ header_t ���ݰ�ͷ
	struct header_t {
		uint16_t len;      //���ܳ���
		uint16_t crc;      //CRCУ��λ
		uint16_t ver;      //�汾��
		uint16_t sign;     //ǩ��
		uint8_t  mainID;   //����ϢmainID
		uint8_t  subID;    //����ϢsubID
		uint8_t  enctype;  //��������
		uint8_t  reserved; //Ԥ��
		uint32_t reqID;
		uint16_t realsize; //�û����ݳ���
	};

	//@@ internal_prev_header_t ���ݰ�ͷ(�ڲ�ʹ��)
	struct internal_prev_header_t {
		uint16_t len;
		int16_t  kicking;
		int32_t  ok;
		int64_t  userID;
		uint32_t ipaddr;             //������ʵIP
		uint8_t  session[SESSIONSZ]; //�û��Ự
		uint8_t  aeskey[AESKEYSZ];   //AES_KEY
#if 0
		uint8_t  servID[SERVIDSZ];   //���Խڵ�ID
#endif
		uint16_t checksum;           //У���CHKSUM
	};

#pragma pack()

	//@@
	static const size_t kHeaderLen = sizeof(header_t);
	static const size_t kPrevHeaderLen = sizeof(internal_prev_header_t);
	static const size_t kMaxPacketSZ = 60 * 1024;
	static const size_t kMinPacketSZ = sizeof(int16_t);

	static const size_t kSessionSZ = sizeof(((internal_prev_header_t*)0)->session);
	static const size_t kAesKeySZ = sizeof(((internal_prev_header_t*)0)->aeskey);
#if 0
	static const size_t kServIDSZ = sizeof(((internal_prev_header_t*)0)->servID);
#endif

	//enword
	static inline int enword(int mainID, int subID) {
		return ((0xFF & mainID) << 8) | (0xFF & subID);
	}
	
	//hiword
	static inline int hiword(int cmd) {
		return (0xFF & (cmd >> 8));
	}

	//loword
	static inline int loword(int cmd) {
		return (0xFF & cmd);
	}

	//getCheckSum ����У���
	static uint16_t getCheckSum(uint8_t const* header, size_t size) {
		uint16_t sum = 0;
		uint16_t const* ptr = (uint16_t const*)header;
		for (size_t i = 0; i < size / 2; ++i) {
			//��ȡuint16��2�ֽ�
			sum += *ptr++;
		}
		if (size % 2) {
			//��ȡuint8��1�ֽ�
			sum += *(uint8_t const*)ptr;
		}
		return sum;
	}

	//setCheckSum ����У���
	static void setCheckSum(internal_prev_header_t* header) {
		uint16_t sum = 0;
		uint16_t* ptr = (uint16_t*)header;
		for (size_t i = 0; i < kPrevHeaderLen / 2 - 1; ++i) {
			//��ȡuint16��2�ֽ�
			sum += *ptr++;
		}
		uint16_t offset = ptr - (uint16_t const*)header;
		assert(offset == offsetof(internal_prev_header_t, checksum));
		//CRCУ��λ
		*ptr = sum;
	}

	//checkCheckSum ����У���
	static bool checkCheckSum(internal_prev_header_t const* header) {
		uint16_t sum = 0;
		uint16_t const* ptr = (uint16_t const*)header;
		for (size_t i = 0; i < kPrevHeaderLen / 2 - 1; ++i) {
			//��ȡuint16��2�ֽ�
			sum += *ptr++;
		}
		uint16_t offset = ptr - (uint16_t const*)header;
		assert(offset == offsetof(internal_prev_header_t, checksum));
		//У��CRC
		return *ptr == sum;
	}

	//pack data[len] to buffer with packet::header_t
	void packMessage(muduo::net::Buffer* buffer, int mainID, int subID, char const* data, size_t len);
	BufferPtr packMessage(int mainID, int subID, char const* data, size_t len);
	
	//pack protobuf to buffer with packet::header_t
	bool packMessage(muduo::net::Buffer* buffer, int mainID, int subID, ::google::protobuf::Message* data);
	BufferPtr packMessage(int mainID, int subID, ::google::protobuf::Message* data);

	//pack data[len] to buffer with packet::internal_prev_header_t
	void packMessage(
		muduo::net::Buffer* buffer,
		int64_t userid,
		std::string const& session,
		std::string const& aeskey,
		uint32_t clientip,
		int16_t kicking,
#if 0
		std::string const& servid,
#endif
		char const* data, size_t len);
	
	BufferPtr packMessage(
		int64_t userid,
		std::string const& session,
		std::string const& aeskey,
		uint32_t clientip,
		int16_t kicking,
#if 0
		std::string const& servid,
#endif
		char const* data, size_t len);

	//pack data[len] to buffer with packet::internal_prev_header_t & packet::header_t
	void packMessage(
		muduo::net::Buffer* buffer,
		int64_t userid,
		std::string const& session,
		std::string const& aeskey,
		uint32_t clientip,
		int16_t kicking,
#if 0
		std::string const& servid,
#endif
		int mainID, int subID,
		char const* data, size_t len);
	
	BufferPtr packMessage(
		int64_t userid,
		std::string const& session,
		std::string const& aeskey,
		uint32_t clientip,
		int16_t kicking,
#if 0
		std::string const& servid,
#endif
		int mainID, int subID,
		char const* data, size_t len);

	//pack protobuf to buffer with packet::internal_prev_header_t & packet::header_t
	bool packMessage(
		muduo::net::Buffer* buffer,
		int64_t userid,
		std::string const& session,
		std::string const& aeskey,
		uint32_t clientip,
		int16_t kicking,
#if 0
		std::string const& servid,
#endif
		int mainID, int subID,
		::google::protobuf::Message* data);

	BufferPtr packMessage(
		int64_t userid,
		std::string const& session,
		std::string const& aeskey,
		uint32_t clientip,
		int16_t kicking,
#if 0
		std::string const& servid,
#endif
		int mainID, int subID,
		::google::protobuf::Message* data);

	static internal_prev_header_t const* get_pre_header(BufferPtr& buf) {
		return (internal_prev_header_t const*)buf->peek();
	}
	
	static header_t const* get_header(BufferPtr& buf) {
		return (header_t const*)(buf->peek() + kPrevHeaderLen);
	}
	
	static uint8_t const* get_msg(header_t const* header) {
		(uint8_t const*)header + kHeaderLen;
	}
	
	static size_t get_msglen(header_t const* header) {
		return header->len - kHeaderLen;
	}

}//namespace packet

#endif