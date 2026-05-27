// thisfile_included.tl — included file for thisfile include test
// thisfile() here should return this file's path, NOT the including file's path

fun get_included_thisfile() {
    ret thisfile()
}
