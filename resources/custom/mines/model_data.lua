local sizes = { "small", "medium", "big" }
local states = { "lit", "unlit" }

for i, size in ipairs(sizes) do
	for j, state in ipairs(states) do
		-- small is 2 units wide
		-- medium is 3 units wide
		-- big is 4 units wide
		model = ModelData()
		model:setName("mine_" .. size .. "_" .. state)
		model:setMesh("custom/mines/ee_mine_"..size..".obj")
		model:setTexture("custom/mines/ee_mine_BaseColor.png")
		model:setSpecular("custom/mines/ee_mine_roughness.png")
		if j == 1 then
			model:setIllumination("custom/mines/ee_mine_illumination.png")
		end
		model:setScale(50 * (4 / (1+i)))
		model:setRadius(100)
	end
end
