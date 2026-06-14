set_project("electronic-shield-door")
set_version("1.0.0")
set_languages("c++17")

add_rules("mode.release", "mode.debug")

add_requires("opencv", { system = true })

target("door_opener")
set_kind("binary")
add_files("src/*.cpp")
add_packages("opencv")

after_build(function(target)
	local pkg = target:pkg("opencv")
	if pkg then
		for _, libfile in ipairs(pkg:get("libfiles")) do
			local libdir = path.directory(libfile)
			local filename = path.filename(libfile)

			local basename = filename:match("^(libopencv_[^%.]+)")

			if basename then
				for _, matched_file in ipairs(os.files(path.join(libdir, basename .. ".so*"))) do
					os.cp(matched_file, target:targetdir())
				end
			end
		end
	end
end)

-- Intercept the install phase and properly enforce a flat directory layout
on_install(function(target)
	local destdir = target:installdir()

	-- FORCE XMake to treat 'dist' as a directory by creating it first
	os.mkdir(destdir)

	-- Copy everything flat inside the newly created directory
	os.cp(path.join(target:targetdir(), "*"), destdir)
end)
