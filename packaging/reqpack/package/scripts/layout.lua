local M = {}

function M.join_path(base, relative)
  if base == nil or base == "" or base == "." then
    return relative
  end
  if relative == nil or relative == "" then
    return base
  end
  if base == "/" then
    return "/" .. relative
  end
  if base:sub(-1) == "/" then
    return base .. relative
  end
  return base .. "/" .. relative
end

function M.shell_quote(value)
  return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

function M.trim(value)
  return (tostring(value):gsub("^%s+", ""):gsub("%s+$", ""))
end

function M.home_dir()
  local home = os.getenv("HOME")
  if home == nil or home == "" then
    error("HOME is not set")
  end
  return home
end

function M.version_identity(metadata)
  return string.format("%s-%s+r%s", tostring(metadata.version), tostring(metadata.release), tostring(metadata.revision))
end

function M.paths(context)
  local home = M.home_dir()
  local local_root = M.join_path(home, ".local")
  local share_root = M.join_path(local_root, "share")
  local package_root = M.join_path(share_root, "prebyte")
  local version_root = M.join_path(package_root, M.version_identity(context.metadata))
  local bin_dir = M.join_path(version_root, "bin")
  local share_dir = M.join_path(version_root, "share")
  local doc_dir = M.join_path(share_dir, "doc")
  local package_doc_dir = M.join_path(doc_dir, "prebyte")
  local stable_bin_dir = M.join_path(local_root, "bin")
  local binary_path = M.join_path(bin_dir, "prebyte")
  local symlink_path = M.join_path(stable_bin_dir, "prebyte")

  return {
    local_root = local_root,
    share_root = share_root,
    package_root = package_root,
    version_root = version_root,
    bin_dir = bin_dir,
    share_dir = share_dir,
    doc_dir = doc_dir,
    package_doc_dir = package_doc_dir,
    stable_bin_dir = stable_bin_dir,
    binary_path = binary_path,
    symlink_path = symlink_path,
  }
end

function M.load_payload_manifest(context)
  local ok, payload = pcall(dofile, M.join_path(context.paths.controlDir, "scripts/payload_files.lua"))
  if not ok then
    return nil, "failed to load payload manifest: " .. tostring(payload)
  end
  return payload
end

function M.ensure_directories(context, directories)
  for _, dir in ipairs(directories) do
    if not context.fs.exists(dir) then
      local ok, result = pcall(context.fs.mkdir, dir)
      if not ok or result == false then
        return false, "failed to create directory: " .. tostring(dir)
      end
    end
  end
  return true
end

function M.update_symlink(context, source, target)
  local command = "ln -sfn " .. M.shell_quote(source) .. " " .. M.shell_quote(target)
  local result = context.exec.run(command)
  if not result.success then
    local message = result.stderr ~= "" and result.stderr or ("failed to create symlink: " .. tostring(target))
    return false, message
  end
  local ok, registered = pcall(context.artifacts.register_symlink, target)
  if not ok or registered == false then
    return false, "failed to register symlink: " .. tostring(target)
  end
  return true
end

function M.read_symlink(context, path)
  local result = context.exec.run("readlink " .. M.shell_quote(path))
  if not result.success then
    return nil
  end
  return M.trim(result.stdout)
end

function M.remove_path(context, path)
  local result = context.exec.run("rm -f " .. M.shell_quote(path))
  if not result.success then
    local message = result.stderr ~= "" and result.stderr or ("failed to remove path: " .. tostring(path))
    return false, message
  end
  return true
end

return M
