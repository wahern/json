#include "json.h"

int main(void) {
	struct json *J;
	int error;

	J = json_open(JSON_F_NONE, &error);
	json_push(J, ".Image.Thumbnail");
	json_setstring(J, "http://www.example.com/image/481989943", "Url");
	json_setnumber(J, 125, ".Height");
	json_setstring(J, "100", ".Width");
	json_pop(J);

	json_setnumber(J, 800, ".Image.Width");
	json_setnumber(J, 600, ".Image.Height");
	json_setstring(J, "View from 15th Floor", ".Image.$", "Title");

	// "IDs": [116, 943, 234, 38793]
	json_setnumber(J, 116, ".IDs[0]");
	json_setnumber(J, 943, ".IDs[#]", json_count(J, ".IDs"));
	json_push(J, ".IDs[#]", json_count(J, ".IDs"));
	json_setnumber(J, 234, ".");
	json_pop(J);
	json_setnumber(J, 38793, ".IDs[3]");

	json_printfile(J, stdout, JSON_F_PRETTY);

	json_close(J);

	return 0;
}
