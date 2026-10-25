#include "mh/io/filesystem_helpers.hpp"

#include <catch2/catch_all.hpp>

#include <filesystem>

using std::filesystem::path;

TEST_CASE("filename_without_extension", "[io][filesystem]")
{
	CHECK(mh::filename_without_extension(path("/foo/bar/baz.txt")) == path("baz"));
	CHECK(mh::filename_without_extension(path("relative/dir/name.ext")) == path("name"));

	// only the LAST extension is dropped
	CHECK(mh::filename_without_extension(path("archive.tar.gz")) == path("archive.tar"));

	// no extension / no directory
	CHECK(mh::filename_without_extension(path("plain")) == path("plain"));
	CHECK(mh::filename_without_extension(path("/dir/only/")) == path(""));

	// dotfiles have no "extension" per std::filesystem
	CHECK(mh::filename_without_extension(path(".gitignore")) == path(".gitignore"));
}

TEST_CASE("replace_filename_keep_extension", "[io][filesystem]")
{
	// the new name adopts the ORIGINAL path's extension...
	CHECK(mh::replace_filename_keep_extension(path("/foo/bar.txt"), path("qux")) == path("/foo/qux.txt"));

	// ...including replacing whatever extension the new name had
	CHECK(mh::replace_filename_keep_extension(path("/foo/bar.txt"), path("qux.md")) == path("/foo/qux.txt"));

	// extensionless original: the new name's extension is stripped too
	CHECK(mh::replace_filename_keep_extension(path("/foo/bar"), path("qux.md")) == path("/foo/qux"));

	// directory is preserved, relative paths work
	CHECK(mh::replace_filename_keep_extension(path("a/b/c.ini"), path("d")) == path("a/b/d.ini"));
}
