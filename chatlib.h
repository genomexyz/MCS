int kirim (int sock, void *buf, socklen_t len)
{
	int pos = 0;
	int size = len;
	int res;
	int err;

	while (pos < len) {
		res = send(sock, buf + pos, size, MSG_NOSIGNAL);
		err = errno;
		if (res == -1) {
			if (err == EPIPE)
				printf("broken pipe for send\n");
			break;
		}
		pos += res;
		size -= res;
	}
	return err;
}

int terima (int sock, void *buf, socklen_t len)
{
	int pos = 0;
	int size = len;
	int res;
	int err;

	while (pos < len) {
		res = recv(sock, buf + pos, size, MSG_NOSIGNAL);
		err = errno;
		if (res == -1 || res ==0) {
			if (err == EPIPE)
				printf("broken pipe for recv\n");
			break;
		}
		pos += res;
		size -= res;
	}
	return err;
}
