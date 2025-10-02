/*
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#include <schannel.h>
#include <security.h>
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")
*/

// Helper: ajoute message d'erreur
static void set_errmsg(char *errmsg, size_t errmsg_size, const char *fmt, ...) {
    if (!errmsg || errmsg_size == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errmsg, errmsg_size, fmt, ap);
    va_end(ap);
}

// Crée socket et connecte au host:port (IPv4/IPv6 via getaddrinfo)
static SOCKET tcp_connect_host(const char *host, const char *port, char *errmsg, size_t errmsg_size) {
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        set_errmsg(errmsg, errmsg_size, "getaddrinfo(%s:%s) failed: %s", host, port, gai_strerror(rc));
        return INVALID_SOCKET;
    }
    SOCKET s = INVALID_SOCKET;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) == 0) break;
        closesocket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (s == INVALID_SOCKET) {
        set_errmsg(errmsg, errmsg_size, "Impossible de se connecter à %s:%s", host, port);
    }
    return s;
}

// Vérification basique du certificat via CertVerifyCertificateChainPolicy
static BOOL verify_server_certificate(PCCERT_CONTEXT pServerCert, char *errmsg, size_t errmsg_size) {
    BOOL ok = FALSE;
    CERT_CHAIN_PARA ChainPara = { sizeof(ChainPara) };
    PCCERT_CHAIN_CONTEXT pChainContext = NULL;
    if (!CertGetCertificateChain(NULL, pServerCert, NULL, pServerCert->hCertStore, &ChainPara, 0, NULL, &pChainContext)) {
        set_errmsg(errmsg, errmsg_size, "CertGetCertificateChain failed");
        return FALSE;
    }
    CERT_CHAIN_POLICY_PARA PolicyPara = { sizeof(PolicyPara) };
    CERT_CHAIN_POLICY_STATUS PolicyStatus = {0};
    if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, pChainContext, &PolicyPara, &PolicyStatus)) {
        set_errmsg(errmsg, errmsg_size, "CertVerifyCertificateChainPolicy error");
        ok = FALSE;
    } else {
        if (PolicyStatus.dwError == 0) ok = TRUE;
        else set_errmsg(errmsg, errmsg_size, "Cert verification failed, error=0x%08x", PolicyStatus.dwError);
    }
    if (pChainContext) CertFreeCertificateChain(pChainContext);
    return ok;
}

// Lecture socket (non TLS) -> renvoyé dans buffer (utilisé pendant handshake initial)
// mais pour TLS on utilisera les appels Schannel/Decrypt
// ---- FONCTION PRINCIPALE ----
// Retourne 0=OK, !=0 erreur
int https_fetch_url_body_schannel(const char *url, char **out_body, size_t *out_len, char *errmsg, size_t errmsg_size) {
    if (!url || !out_body || !out_len) {
        set_errmsg(errmsg, errmsg_size, "Paramètres invalides");
        return -1;
    }
    *out_body = NULL;
    *out_len = 0;

    // simple parsing url: https://host[:port]/path
    if (strncmp(url, "https://", 8) != 0) {
        set_errmsg(errmsg, errmsg_size, "URL non https");
        return -1;
    }
    const char *u = url + 8;
    char host[256] = {0};
    char path[1024] = "/";
    const char *slash = strchr(u, '/');
    if (slash) {
        size_t hlen = (size_t)(slash - u);
        if (hlen >= sizeof(host)) hlen = sizeof(host)-1;
        strncpy(host, u, hlen);
        host[hlen] = '\0';
        strncpy(path, slash, sizeof(path)-1);
        path[sizeof(path)-1] = '\0';
    } else {
        strncpy(host, u, sizeof(host)-1);
        path[0] = '/';
        path[1] = '\0';
    }
    // port default 443, allow host:port
    char port_str[16] = "443";
    char host_only[256];
    strncpy(host_only, host, sizeof(host_only)-1);
    host_only[sizeof(host_only)-1] = '\0';
    char *colon = strchr(host_only, ':');
    if (colon) {
        *colon = '\0';
        strncpy(port_str, colon+1, sizeof(port_str)-1);
        port_str[sizeof(port_str)-1] = '\0';
    }

    // init winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        set_errmsg(errmsg, errmsg_size, "WSAStartup failed");
        return -1;
    }

    SOCKET sock = tcp_connect_host(host_only, port_str, errmsg, errmsg_size);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return -1;
    }

    // Schannel setup
    CredHandle hCred = {0};
    TimeStamp tsExpiry;
    SCHANNEL_CRED schCred = {0};
    schCred.dwVersion = SCHANNEL_CRED_VERSION;
    schCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;//SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_CLIENT;  // protocol TLS1.2 uniquement
    schCred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION; // laisser win vérifier (on fera vérif additionnelle)

    SECURITY_STATUS scRet = AcquireCredentialsHandleA(
        NULL,
        UNISP_NAME_A,
        SECPKG_CRED_OUTBOUND,
        NULL,
        &schCred,
        NULL,
        NULL,
        &hCred,
        &tsExpiry);
    if (scRet != SEC_E_OK) {
        set_errmsg(errmsg, errmsg_size, "AcquireCredentialsHandleA failed: 0x%08x", scRet);
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    // Init security context (handshake)
    CtxtHandle hContext;
    SecBuffer outBufDescBuffer;
    SecBufferDesc outBufDesc;
    SecBuffer inBuf[2];
    SecBufferDesc inBufDesc;
    DWORD ctxtAttr = 0;
    BOOL fContinue = FALSE;
    BOOL haveContext = FALSE;

    // first call InitializeSecurityContext with no context
    SecBuffer outSecBuf;
    SecBufferDesc outSecDesc;
    outSecBuf.BufferType = SECBUFFER_TOKEN;
    outSecBuf.pvBuffer = NULL;
    outSecBuf.cbBuffer = 0;
    outSecDesc.ulVersion = SECBUFFER_VERSION;
    outSecDesc.cBuffers = 1;
    outSecDesc.pBuffers = &outSecBuf;

    scRet = InitializeSecurityContextA(
        &hCred,
        NULL,
        (SEC_CHAR*)host_only,
        ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_STREAM |
        ISC_REQ_ALLOCATE_MEMORY,
        0, SECURITY_NATIVE_DREP,
        NULL, 0,
        &hContext,
        &outSecDesc,
        &ctxtAttr,
        &tsExpiry);

    if (scRet == SEC_I_CONTINUE_NEEDED || scRet == SEC_E_OK) {
        if (outSecBuf.cbBuffer != 0 && outSecBuf.pvBuffer != NULL) {
            // send token to server
            int sent = send(sock, (char*)outSecBuf.pvBuffer, (int)outSecBuf.cbBuffer, 0);
            FreeContextBuffer(outSecBuf.pvBuffer);
            outSecBuf.pvBuffer = NULL;
            if (sent == SOCKET_ERROR) {
                set_errmsg(errmsg, errmsg_size, "send() initial token failed");
                DeleteSecurityContext(&hContext);
                FreeCredentialsHandle(&hCred);
                closesocket(sock);
                WSACleanup();
                return -1;
            }
        }
    } else {
        set_errmsg(errmsg, errmsg_size, "InitializeSecurityContextA failed: 0x%08x", scRet);
        FreeCredentialsHandle(&hCred);
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    // loop to complete handshake (exchange tokens)
    // We will read data from socket and feed it to InitializeSecurityContext until we get SEC_E_OK
    char recvbuf[MAX_COMMAND_LEN];
    int recvlen;
    // We'll store leftover bytes between iterations
    unsigned char *in_buffer = NULL;
    size_t in_buf_len = 0;

    while (scRet == SEC_I_CONTINUE_NEEDED) {
        // receive from socket
        recvlen = recv(sock, recvbuf, sizeof(recvbuf), 0);
        if (recvlen <= 0) {
            set_errmsg(errmsg, errmsg_size, "recv() during handshake failed");
            if (in_buffer) free(in_buffer);
            DeleteSecurityContext(&hContext);
            FreeCredentialsHandle(&hCred);
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        // append to in_buffer
        unsigned char *tmp = realloc(in_buffer, in_buf_len + recvlen);
        if (!tmp) { set_errmsg(errmsg, errmsg_size, "realloc failed"); if (in_buffer) free(in_buffer); return -1; }
        in_buffer = tmp;
        memcpy(in_buffer + in_buf_len, recvbuf, recvlen);
        in_buf_len += recvlen;

        // prepare SecBufferDesc in
        inBuf[0].pvBuffer = in_buffer;
        inBuf[0].cbBuffer = (unsigned long)in_buf_len;
        inBuf[0].BufferType = SECBUFFER_TOKEN;
        inBuf[1].pvBuffer = NULL;
        inBuf[1].cbBuffer = 0;
        inBuf[1].BufferType = SECBUFFER_EMPTY;
        inBufDesc.ulVersion = SECBUFFER_VERSION;
        inBufDesc.cBuffers = 2;
        inBufDesc.pBuffers = inBuf;

        outSecBuf.pvBuffer = NULL;
        outSecBuf.cbBuffer = 0;
        outSecBuf.BufferType = SECBUFFER_TOKEN;
        outSecDesc.ulVersion = SECBUFFER_VERSION;
        outSecDesc.cBuffers = 1;
        outSecDesc.pBuffers = &outSecBuf;

        scRet = InitializeSecurityContextA(
            &hCred,
            &hContext,
            (SEC_CHAR*)host_only,
            ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_STREAM | ISC_REQ_ALLOCATE_MEMORY,
            0, SECURITY_NATIVE_DREP,
            &inBufDesc,
            0,
            &hContext,
            &outSecDesc,
            &ctxtAttr,
            &tsExpiry);

        // If outSecBuf has data, send it
        if (outSecBuf.cbBuffer != 0 && outSecBuf.pvBuffer) {
            int sent = send(sock, (char*)outSecBuf.pvBuffer, (int)outSecBuf.cbBuffer, 0);
            FreeContextBuffer(outSecBuf.pvBuffer);
            outSecBuf.pvBuffer = NULL;
            if (sent == SOCKET_ERROR) {
                set_errmsg(errmsg, errmsg_size, "send() during handshake failed");
                if (in_buffer) free(in_buffer);
                DeleteSecurityContext(&hContext);
                FreeCredentialsHandle(&hCred);
                closesocket(sock);
                WSACleanup();
                return -1;
            }
        }

        // After InitializeSecurityContext, the inBuf may contain leftover bytes (SECBUFFER_EXTRA)
        // Find SECBUFFER_EXTRA, move data to front
        for (int i = 0; i < inBufDesc.cBuffers; i++) {
            if (inBufDesc.pBuffers[i].BufferType == SECBUFFER_EXTRA) {
                // extra data begins at (unsigned char*)inBufDesc.pBuffers[i].pvBuffer, length cbBuffer
                unsigned char *extra_ptr = (unsigned char*)inBufDesc.pBuffers[i].pvBuffer;
                size_t extra_len = inBufDesc.pBuffers[i].cbBuffer;
                // allocate new buffer of extra_len and copy
                unsigned char *newbuf = NULL;
                if (extra_len > 0) {
                    newbuf = malloc(extra_len);
                    if (!newbuf) { set_errmsg(errmsg, errmsg_size, "malloc failed"); if (in_buffer) free(in_buffer); return -1; }
                    memcpy(newbuf, extra_ptr, extra_len);
                }
                // replace in_buffer with extra
                if (in_buffer) free(in_buffer);
                in_buffer = newbuf;
                in_buf_len = extra_len;
                break;
            }
        }

        if (scRet == SEC_E_INCOMPLETE_MESSAGE) {
            // Il manque des données, garder le buffer tel quel et refaire recv()
            continue;
        }

        // if no SECBUFFER_EXTRA, then all consumed -> free buffer
        BOOL had_extra = FALSE;
        for (int i=0;i<inBufDesc.cBuffers;i++){
            if (inBufDesc.pBuffers[i].BufferType == SECBUFFER_EXTRA) { had_extra = TRUE; break; }
        }
        if (!had_extra) {
            if (in_buffer) { free(in_buffer); in_buffer = NULL; in_buf_len = 0; }
        }
    } // end handshake loop

    if (scRet != SEC_E_OK) {
        set_errmsg(errmsg, errmsg_size, "TLS handshake failed: 0x%08x", scRet);
        if (in_buffer) free(in_buffer);
        DeleteSecurityContext(&hContext);
        FreeCredentialsHandle(&hCred);
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    // OPTIONAL: certificate verification (retrieve server cert from context)
    // Query context sizes to get remote certificate chain via QueryContextAttributes(SECPKG_ATTR_REMOTE_CERT_CONTEXT)
    PCCERT_CONTEXT pRemoteCertContext = NULL;
    QueryContextAttributes(&hContext, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &pRemoteCertContext);
    if (pRemoteCertContext) {
        if (!verify_server_certificate(pRemoteCertContext, errmsg, errmsg_size)) {
            // verification failed
            CertFreeCertificateContext(pRemoteCertContext);
            DeleteSecurityContext(&hContext);
            FreeCredentialsHandle(&hCred);
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        CertFreeCertificateContext(pRemoteCertContext);
    }

    // Build HTTP GET request (HTTP/1.0 to simplify closing)
    char req[2048];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "User-Agent: fr-simplecode/1.0\r\n"
             "\r\n",
             path, host_only);

    // We need to use ApplyControlToken/EncryptMessage? Simpler: use InitializeSecurityContext
    // Use EncryptMessage with the security context to create TLS record bytes to send.
    // Prepare SecBuffer for encryption
    SecPkgContext_StreamSizes sizes = {0};
    if (QueryContextAttributes(&hContext, SECPKG_ATTR_STREAM_SIZES, &sizes) != SEC_E_OK) {
        set_errmsg(errmsg, errmsg_size, "QueryContextAttributes STREAM_SIZES failed");
        DeleteSecurityContext(&hContext);
        FreeCredentialsHandle(&hCred);
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    // Build outbound buffer: header | data | trailer
    DWORD header_size = sizes.cbHeader;
    DWORD trailer_size = sizes.cbTrailer;
    DWORD packet_len = header_size + (DWORD)strlen(req) + trailer_size;
    unsigned char *send_packet = malloc(packet_len);
    if (!send_packet) { set_errmsg(errmsg, errmsg_size, "malloc fail"); DeleteSecurityContext(&hContext); FreeCredentialsHandle(&hCred); closesocket(sock); WSACleanup(); return -1; }
    // arrange SecBuffers
    SecBuffer secbufs[4];
    secbufs[0].pvBuffer = send_packet;
    secbufs[0].cbBuffer = header_size;
    secbufs[0].BufferType = SECBUFFER_STREAM_HEADER;
    secbufs[1].pvBuffer = send_packet + header_size;
    secbufs[1].cbBuffer = (unsigned long)strlen(req);
    secbufs[1].BufferType = SECBUFFER_DATA;
    secbufs[2].pvBuffer = send_packet + header_size + strlen(req);
    secbufs[2].cbBuffer = trailer_size;
    secbufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
    secbufs[3].pvBuffer = NULL;
    secbufs[3].cbBuffer = 0;
    secbufs[3].BufferType = SECBUFFER_EMPTY;
    SecBufferDesc message;
    message.ulVersion = SECBUFFER_VERSION;
    message.cBuffers = 4;
    message.pBuffers = secbufs;
    // copy plaintext into data buffer
    memcpy(secbufs[1].pvBuffer, req, secbufs[1].cbBuffer);

    // Encrypt
    scRet = EncryptMessage(&hContext, 0, &message, 0);
    if (scRet != SEC_E_OK) {
        set_errmsg(errmsg, errmsg_size, "EncryptMessage failed: 0x%08x", scRet);
        free(send_packet); DeleteSecurityContext(&hContext); FreeCredentialsHandle(&hCred); closesocket(sock); WSACleanup(); return -1;
    }
    // send total (header + data + trailer actual sizes are in secbufs[0].cbBuffer etc.)
    int tosend = secbufs[0].cbBuffer + secbufs[1].cbBuffer + secbufs[2].cbBuffer;
    int sent = send(sock, (char*)secbufs[0].pvBuffer, tosend, 0);
    free(send_packet);
    if (sent == SOCKET_ERROR) {
        set_errmsg(errmsg, errmsg_size, "send(request) failed");
        DeleteSecurityContext(&hContext); FreeCredentialsHandle(&hCred); closesocket(sock); WSACleanup(); return -1;
    }

    // Now read response frames until connection closes; decrypt with DecryptMessage
    // We'll accumulate plaintext bytes into a dynamic buffer.
    unsigned char recv_enc[MAX_COMMAND_LEN];
    unsigned char *plain_buf = NULL;
    size_t plain_len = 0;

    // Buffer to store encrypted stream unread bytes
    unsigned char *enc_buf = NULL;
    size_t enc_len = 0;

    while (1) {
        int r = recv(sock, (char*)recv_enc, sizeof(recv_enc), 0);
        if (r <= 0) break;
        unsigned char *tmp2 = realloc(enc_buf, enc_len + r);
        if (!tmp2) { set_errmsg(errmsg, errmsg_size, "realloc fail"); if (enc_buf) free(enc_buf); if (plain_buf) free(plain_buf); DeleteSecurityContext(&hContext); FreeCredentialsHandle(&hCred); closesocket(sock); WSACleanup(); return -1; }
        enc_buf = tmp2;
        memcpy(enc_buf + enc_len, recv_enc, r);
        enc_len += r;

        // try to decrypt as much as possible
        BOOL keep_trying = TRUE;
        while (keep_trying) {
            SecBuffer inSecBufs[4];
            SecBufferDesc inMsg;
            inSecBufs[0].pvBuffer = enc_buf;
            inSecBufs[0].cbBuffer = (unsigned long)enc_len;
            inSecBufs[0].BufferType = SECBUFFER_DATA;
            inSecBufs[1].pvBuffer = NULL;
            inSecBufs[1].cbBuffer = 0;
            inSecBufs[1].BufferType = SECBUFFER_EMPTY;
            inSecBufs[2].pvBuffer = NULL;
            inSecBufs[2].cbBuffer = 0;
            inSecBufs[2].BufferType = SECBUFFER_EMPTY;
            inSecBufs[3].pvBuffer = NULL;
            inSecBufs[3].cbBuffer = 0;
            inSecBufs[3].BufferType = SECBUFFER_EMPTY;
            inMsg.ulVersion = SECBUFFER_VERSION;
            inMsg.cBuffers = 4;
            inMsg.pBuffers = inSecBufs;

            scRet = DecryptMessage(&hContext, &inMsg, 0, NULL);
            if (scRet == SEC_E_OK) {
                // find SECBUFFER_DATA for plaintext
                for (int i = 0; i < inMsg.cBuffers; i++) {
                    if (inMsg.pBuffers[i].BufferType == SECBUFFER_DATA && inMsg.pBuffers[i].cbBuffer > 0) {
                        unsigned char *data_ptr = (unsigned char*)inMsg.pBuffers[i].pvBuffer;
                        size_t data_len = inMsg.pBuffers[i].cbBuffer;
                        unsigned char *nb = realloc(plain_buf, plain_len + data_len);
                        if (!nb) { set_errmsg(errmsg, errmsg_size, "realloc plain fail"); if (enc_buf) free(enc_buf); if (plain_buf) free(plain_buf); DeleteSecurityContext(&hContext); FreeCredentialsHandle(&hCred); closesocket(sock); WSACleanup(); return -1; }
                        plain_buf = nb;
                        memcpy(plain_buf + plain_len, data_ptr, data_len);
                        plain_len += data_len;
                    }
                }
                // If there is SECBUFFER_EXTRA, shift remaining encrypted bytes to front
                BOOL had_extra = FALSE;
                for (int i=0;i<inMsg.cBuffers;i++){
                    if (inMsg.pBuffers[i].BufferType == SECBUFFER_EXTRA && inMsg.pBuffers[i].cbBuffer > 0) {
                        unsigned char *extra_ptr = (unsigned char*)inMsg.pBuffers[i].pvBuffer;
                        size_t extra_len = inMsg.pBuffers[i].cbBuffer;
                        // move extra to front
                        unsigned char *new_enc = malloc(extra_len);
                        if (!new_enc) { set_errmsg(errmsg, errmsg_size, "malloc fail"); if (enc_buf) free(enc_buf); if (plain_buf) free(plain_buf); DeleteSecurityContext(&hContext); FreeCredentialsHandle(&hCred); closesocket(sock); WSACleanup(); return -1; }
                        memcpy(new_enc, extra_ptr, extra_len);
                        free(enc_buf);
                        enc_buf = new_enc;
                        enc_len = extra_len;
                        had_extra = TRUE;
                        break;
                    }
                }
                if (!had_extra) {
                    free(enc_buf);
                    enc_buf = NULL;
                    enc_len = 0;
                    keep_trying = FALSE;
                } else {
                    // continue loop: we have extra encrypted bytes to try decrypt again
                    keep_trying = (enc_len > 0);
                    if (!keep_trying) break;
                }
            } else if (scRet == SEC_E_INCOMPLETE_MESSAGE) {
                // need more data, break to outer recv loop
                keep_trying = FALSE;
            } else if (scRet == SEC_I_CONTEXT_EXPIRED) {
                // session closed cleanly
                keep_trying = FALSE;
            } else {
                set_errmsg(errmsg, errmsg_size, "DecryptMessage failed: 0x%08x", scRet);
                if (enc_buf) free(enc_buf);
                if (plain_buf) free(plain_buf);
                DeleteSecurityContext(&hContext);
                FreeCredentialsHandle(&hCred);
                closesocket(sock);
                WSACleanup();
                return -1;
            }
        } // end inner decrypt loop
    } // end recv loop

    // close TLS
    // send close notify
    SecBuffer closeBuf;
    SecBufferDesc closeDesc;
    closeBuf.BufferType = SECBUFFER_TOKEN;
    closeBuf.pvBuffer = NULL;
    closeBuf.cbBuffer = 0;
    closeDesc.ulVersion = SECBUFFER_VERSION;
    closeDesc.cBuffers = 1;
    closeDesc.pBuffers = &closeBuf;
    ApplyControlToken(&hContext, &closeDesc);

    // cleanup
    DeleteSecurityContext(&hContext);
    FreeCredentialsHandle(&hCred);
    closesocket(sock);
    WSACleanup();

    if (!plain_buf || plain_len == 0) {
        if (plain_buf) free(plain_buf);
        set_errmsg(errmsg, errmsg_size, "Aucun contenu reçu");
        return -1;
    }

    // plain_buf contient headers+body. find "\r\n\r\n"
    char *headers_end = NULL;
    for (size_t i=0;i+3<plain_len;i++){
        if (plain_buf[i]=='\r' && plain_buf[i+1]=='\n' && plain_buf[i+2]=='\r' && plain_buf[i+3]=='\n') {
            headers_end = (char*)(plain_buf + i + 4);
            size_t body_len = plain_len - (i + 4);
            char *body_copy = malloc(body_len + 1);
            if (!body_copy) { free(plain_buf); set_errmsg(errmsg, errmsg_size, "malloc fail"); return -1; }
            memcpy(body_copy, headers_end, body_len);
            body_copy[body_len] = '\0';
            *out_body = body_copy;
            *out_len = body_len;
            free(plain_buf);
            return 0;
        }
    }

    // if not found, return full plaintext
    char *all_copy = malloc(plain_len + 1);
    if (!all_copy) { free(plain_buf); set_errmsg(errmsg, errmsg_size, "malloc fail"); return -1; }
    memcpy(all_copy, plain_buf, plain_len);
    all_copy[plain_len] = '\0';
    *out_body = all_copy;
    *out_len = plain_len;
    free(plain_buf);
    return 0;
}
