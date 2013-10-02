/* Generate the first example JSON snippet from RFC 4627:
 *
 * {
 * 	"Image": {
 * 		"Width":  800,
 * 		"Height": 600,
 * 		"Title":  "View from 15th Floor",
 * 		"Thumbnail": {
 * 			"Url":    "http://www.example.com/image
 * 			"Height": 125,
 * 			"Width":  "100"
 * 		},
 * 		"IDs": [116, 943, 234, 38793]
 * 	}
 * }
 */
#include "json.h"

int main(void) {
	struct json *J;
	int error;

	J = json_open(JSON_F_NONE, &error);

	json_push(J, ".Image.Thumbnail");
	/* automatically instantiates . as an object, .Image as an object,
	 * and .Image.Thumbnail as a null value. .Image.Thumbnail is now the
	 * root node for path expressions.
	 */

	json_setstring(J, "http://www.example.com/image/481989943", "Url");
	/* automatically converts .Image.Thumbnail to an object and
	 * instantiates .Image.Thumbnail.Url to a string
	 */

	json_setnumber(J, 125, ".Height");
	json_setstring(J, "100", ".Width");

	json_pop(J);
	/* Our root node for path expressions is again the document root */

	json_setnumber(J, 800, ".Image.Width");
	json_setnumber(J, 600, ".Image.Height");

	json_setstring(J, "View from 15th Floor", ".Image.$", "Title");
	/* $ interpolates a string into the path expression */

	json_setnumber(J, 116, ".IDs[0]");
	/* .IDs is instantiated as an array and the number 116 set to the
	 * 0th index
	 */

	json_setnumber(J, 943, ".IDs[#]", json_count(J, ".IDs"));
	/* # interpolates integers into the path expression. json_count
	 * returns the array size of .IDs as an int, which should be 1
	 */

	json_push(J, ".IDs[#]", json_count(J, ".IDs"));
	json_setnumber(J, 234, ".");
	json_pop(J);
	json_setnumber(J, 38793, ".IDs[3]");

	json_printfile(J, stdout, JSON_F_PRETTY);
	/* The JSON_F_PRETTY flag instructs the composer to print one value
	 * per line, and to indent each line with tabs according to its
	 * nested level
	 */

	json_close(J);

	return 0;
}
