/*
	This file is part of Android File Transfer For Linux.
	Copyright (C) 2015-2020  Vladimir Menshakov

	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License,
	or (at your option) any later version.

	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <mtp/mtpz/TrustedApp.h>
#include <mtp/ptp/Session.h>
#include <mtp/log.h>
#include <mutex>

#ifdef MTPZ_ENABLED
#	include <openssl/bio.h>
#	include <openssl/bn.h>
#	include <openssl/crypto.h>
#	include <openssl/rsa.h>
#endif

namespace mtp
{
	std::once_flag crypto_init;

	TrustedApp::TrustedApp(const SessionPtr & session, const std::string &mtpzDataPath):
		_session(session), _keys(LoadKeys(mtpzDataPath))
	{}

	TrustedApp::~TrustedApp()
	{ }

#ifdef MTPZ_ENABLED

	struct TrustedApp::Keys
	{
		ByteArray skey; //session key
		BIGNUM *exp, *mod, *pkey;
		RSA * rsa;
		ByteArray certificate;

		Keys(): exp(), mod(), pkey(), rsa(RSA_new())
		{ }

		void Update()
		{
			if (RSA_set0_key(rsa, mod, exp, pkey))
			{
				debug("created RSA key");
				mod = exp = pkey = NULL;
			}
			else
				throw std::runtime_error("failed to create RSA key");
		}

		~Keys() {
			if (exp)
				BN_free(exp);
			if (mod)
				BN_free(mod);
			if (pkey)
				BN_free(pkey);
			if (rsa)
				RSA_free(rsa);
		}

		static ByteArray FromHex(char *buf, size_t bufsize)
		{
			while(bufsize > 0 && isspace(buf[bufsize - 1]))
				--bufsize;
			buf[bufsize] = 0;

			long size = 0;
			u8 * ptr = OPENSSL_hexstr2buf(buf, &size);
			if (!ptr)
				throw std::runtime_error("hex decoding failed");
			ByteArray data(ptr, ptr + size);
			OPENSSL_free(ptr);
			return data;
		}
	};

	bool TrustedApp::Probe(const SessionPtr & session)
	{
		auto & di = session->GetDeviceInfo();
		bool supported =
			di.Supports(OperationCode::SendWMDRMPDAppRequest) &&
			di.Supports(OperationCode::GetWMDRMPDAppResponse) &&
			di.Supports(OperationCode::EnableTrustedFilesOperations) &&
			di.Supports(OperationCode::DisableTrustedFilesOperations) &&
			di.Supports(OperationCode::EndTrustedAppSession);

		debug("MTPZ supported: " , supported? "yes": "no");
		return supported;
	}

	bool TrustedApp::Authenticate()
	{
		_session->RunTransaction(Session::DefaultTimeout, OperationCode::EndTrustedAppSession);
		return false;
	}

	TrustedApp::KeysPtr TrustedApp::LoadKeys(const std::string & path)
	{
		BIO * bio = BIO_new_file(path.c_str(), "rt");
		if (bio == NULL) {
			error("could not open .mtpz-data");
			return nullptr;
		}

		auto keys = std::make_shared<Keys>();

		try
		{
			char buf[4096];
			//public exp
			BIO_gets(bio, buf, sizeof(buf));
			if (BN_hex2bn(&keys->exp, buf) <= 0)
				throw std::runtime_error("can't read public exponent");

			//session key
			auto r = BIO_gets(bio, buf, sizeof(buf));
			if (r <= 0)
				throw std::runtime_error("BIO_gets: short read");
			keys->skey = Keys::FromHex(buf, r);

			//public mod
			BIO_gets(bio, buf, sizeof(buf));
			if (BN_hex2bn(&keys->mod, buf) <= 0)
				throw std::runtime_error("can't read public modulus");

			//private exponent
			BIO_gets(bio, buf, sizeof(buf));
			if (BN_hex2bn(&keys->pkey, buf) <= 0)
				throw std::runtime_error("can't read private key");

			r = BIO_gets(bio, buf, sizeof(buf));
			if (r <= 0)
				throw std::runtime_error("BIO_gets: short read");
			keys->certificate = Keys::FromHex(buf, r);
			//HexDump("certificate", keys->certificate);
		}
		catch(...)
		{ BIO_free(bio); throw; }

		BIO_free(bio);
		keys->Update();

		return keys;
	}

#else

	bool TrustedApp::Probe(const SessionPtr & session)
	{ return false; }

	TrustedApp::KeysPtr TrustedApp::LoadKeys(const std::string & path)
	{ return nullptr; }

	bool TrustedApp::Authenticate()
	{ return false; }

#endif
}
